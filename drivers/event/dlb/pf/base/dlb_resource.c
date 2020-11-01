/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#include "dlb_hw_types.h"
#include "../../dlb_user.h"
#include "dlb_resource.h"
#include "dlb_osdep.h"
#include "dlb_osdep_bitmap.h"
#include "dlb_osdep_types.h"
#include "dlb_regs.h"

void dlb_disable_dp_vasr_feature(struct dlb_hw *hw)
{
	union dlb_dp_dir_csr_ctrl r0;

	r0.val = DLB_CSR_RD(hw, DLB_DP_DIR_CSR_CTRL);

	r0.field.cfg_vasr_dis = 1;

	DLB_CSR_WR(hw, DLB_DP_DIR_CSR_CTRL, r0.val);
}

void dlb_enable_excess_tokens_alarm(struct dlb_hw *hw)
{
	union dlb_chp_cfg_chp_csr_ctrl r0;

	r0.val = DLB_CSR_RD(hw, DLB_CHP_CFG_CHP_CSR_CTRL);

	r0.val |= 1 << DLB_CHP_CFG_EXCESS_TOKENS_SHIFT;

	DLB_CSR_WR(hw, DLB_CHP_CFG_CHP_CSR_CTRL, r0.val);
}

void dlb_hw_enable_sparse_ldb_cq_mode(struct dlb_hw *hw)
{
	union dlb_sys_cq_mode r0;

	r0.val = DLB_CSR_RD(hw, DLB_SYS_CQ_MODE);

	r0.field.ldb_cq64 = 1;

	DLB_CSR_WR(hw, DLB_SYS_CQ_MODE, r0.val);
}

void dlb_hw_enable_sparse_dir_cq_mode(struct dlb_hw *hw)
{
	union dlb_sys_cq_mode r0;

	r0.val = DLB_CSR_RD(hw, DLB_SYS_CQ_MODE);

	r0.field.dir_cq64 = 1;

	DLB_CSR_WR(hw, DLB_SYS_CQ_MODE, r0.val);
}

void dlb_hw_disable_pf_to_vf_isr_pend_err(struct dlb_hw *hw)
{
	union dlb_sys_sys_alarm_int_enable r0;

	r0.val = DLB_CSR_RD(hw, DLB_SYS_SYS_ALARM_INT_ENABLE);

	r0.field.pf_to_vf_isr_pend_error = 0;

	DLB_CSR_WR(hw, DLB_SYS_SYS_ALARM_INT_ENABLE, r0.val);
}

void dlb_hw_get_num_resources(struct dlb_hw *hw,
			      struct dlb_get_num_resources_args *arg)
{
	struct dlb_function_resources *rsrcs;
	struct dlb_bitmap *map;

	rsrcs = &hw->pf;

	arg->num_sched_domains = rsrcs->num_avail_domains;

	arg->num_ldb_queues = rsrcs->num_avail_ldb_queues;

	arg->num_ldb_ports = rsrcs->num_avail_ldb_ports;

	arg->num_dir_ports = rsrcs->num_avail_dir_pq_pairs;

	map = rsrcs->avail_aqed_freelist_entries;

	arg->num_atomic_inflights = dlb_bitmap_count(map);

	arg->max_contiguous_atomic_inflights =
		dlb_bitmap_longest_set_range(map);

	map = rsrcs->avail_hist_list_entries;

	arg->num_hist_list_entries = dlb_bitmap_count(map);

	arg->max_contiguous_hist_list_entries =
		dlb_bitmap_longest_set_range(map);

	map = rsrcs->avail_qed_freelist_entries;

	arg->num_ldb_credits = dlb_bitmap_count(map);

	arg->max_contiguous_ldb_credits = dlb_bitmap_longest_set_range(map);

	map = rsrcs->avail_dqed_freelist_entries;

	arg->num_dir_credits = dlb_bitmap_count(map);

	arg->max_contiguous_dir_credits = dlb_bitmap_longest_set_range(map);

	arg->num_ldb_credit_pools = rsrcs->num_avail_ldb_credit_pools;

	arg->num_dir_credit_pools = rsrcs->num_avail_dir_credit_pools;
}

static void dlb_init_fn_rsrc_lists(struct dlb_function_resources *rsrc)
{
	dlb_list_init_head(&rsrc->avail_domains);
	dlb_list_init_head(&rsrc->used_domains);
	dlb_list_init_head(&rsrc->avail_ldb_queues);
	dlb_list_init_head(&rsrc->avail_ldb_ports);
	dlb_list_init_head(&rsrc->avail_dir_pq_pairs);
	dlb_list_init_head(&rsrc->avail_ldb_credit_pools);
	dlb_list_init_head(&rsrc->avail_dir_credit_pools);
}

static void dlb_init_domain_rsrc_lists(struct dlb_domain *domain)
{
	dlb_list_init_head(&domain->used_ldb_queues);
	dlb_list_init_head(&domain->used_ldb_ports);
	dlb_list_init_head(&domain->used_dir_pq_pairs);
	dlb_list_init_head(&domain->used_ldb_credit_pools);
	dlb_list_init_head(&domain->used_dir_credit_pools);
	dlb_list_init_head(&domain->avail_ldb_queues);
	dlb_list_init_head(&domain->avail_ldb_ports);
	dlb_list_init_head(&domain->avail_dir_pq_pairs);
	dlb_list_init_head(&domain->avail_ldb_credit_pools);
	dlb_list_init_head(&domain->avail_dir_credit_pools);
}

int dlb_resource_init(struct dlb_hw *hw)
{
	struct dlb_list_entry *list;
	unsigned int i;

	/* For optimal load-balancing, ports that map to one or more QIDs in
	 * common should not be in numerical sequence. This is application
	 * dependent, but the driver interleaves port IDs as much as possible
	 * to reduce the likelihood of this. This initial allocation maximizes
	 * the average distance between an ID and its immediate neighbors (i.e.
	 * the distance from 1 to 0 and to 2, the distance from 2 to 1 and to
	 * 3, etc.).
	 */
	u32 init_ldb_port_allocation[DLB_MAX_NUM_LDB_PORTS] = {
		0,  31, 62, 29, 60, 27, 58, 25, 56, 23, 54, 21, 52, 19, 50, 17,
		48, 15, 46, 13, 44, 11, 42,  9, 40,  7, 38,  5, 36,  3, 34, 1,
		32, 63, 30, 61, 28, 59, 26, 57, 24, 55, 22, 53, 20, 51, 18, 49,
		16, 47, 14, 45, 12, 43, 10, 41,  8, 39,  6, 37,  4, 35,  2, 33
	};

	/* Zero-out resource tracking data structures */
	memset(&hw->rsrcs, 0, sizeof(hw->rsrcs));
	memset(&hw->pf, 0, sizeof(hw->pf));

	dlb_init_fn_rsrc_lists(&hw->pf);

	for (i = 0; i < DLB_MAX_NUM_DOMAINS; i++) {
		memset(&hw->domains[i], 0, sizeof(hw->domains[i]));
		dlb_init_domain_rsrc_lists(&hw->domains[i]);
		hw->domains[i].parent_func = &hw->pf;
	}

	/* Give all resources to the PF driver */
	hw->pf.num_avail_domains = DLB_MAX_NUM_DOMAINS;
	for (i = 0; i < hw->pf.num_avail_domains; i++) {
		list = &hw->domains[i].func_list;

		dlb_list_add(&hw->pf.avail_domains, list);
	}

	hw->pf.num_avail_ldb_queues = DLB_MAX_NUM_LDB_QUEUES;
	for (i = 0; i < hw->pf.num_avail_ldb_queues; i++) {
		list = &hw->rsrcs.ldb_queues[i].func_list;

		dlb_list_add(&hw->pf.avail_ldb_queues, list);
	}

	hw->pf.num_avail_ldb_ports = DLB_MAX_NUM_LDB_PORTS;
	for (i = 0; i < hw->pf.num_avail_ldb_ports; i++) {
		struct dlb_ldb_port *port;

		port = &hw->rsrcs.ldb_ports[init_ldb_port_allocation[i]];

		dlb_list_add(&hw->pf.avail_ldb_ports, &port->func_list);
	}

	hw->pf.num_avail_dir_pq_pairs = DLB_MAX_NUM_DIR_PORTS;
	for (i = 0; i < hw->pf.num_avail_dir_pq_pairs; i++) {
		list = &hw->rsrcs.dir_pq_pairs[i].func_list;

		dlb_list_add(&hw->pf.avail_dir_pq_pairs, list);
	}

	hw->pf.num_avail_ldb_credit_pools = DLB_MAX_NUM_LDB_CREDIT_POOLS;
	for (i = 0; i < hw->pf.num_avail_ldb_credit_pools; i++) {
		list = &hw->rsrcs.ldb_credit_pools[i].func_list;

		dlb_list_add(&hw->pf.avail_ldb_credit_pools, list);
	}

	hw->pf.num_avail_dir_credit_pools = DLB_MAX_NUM_DIR_CREDIT_POOLS;
	for (i = 0; i < hw->pf.num_avail_dir_credit_pools; i++) {
		list = &hw->rsrcs.dir_credit_pools[i].func_list;

		dlb_list_add(&hw->pf.avail_dir_credit_pools, list);
	}

	/* There are 5120 history list entries, which allows us to overprovision
	 * the inflight limit (4096) by 1k.
	 */
	if (dlb_bitmap_alloc(hw,
			     &hw->pf.avail_hist_list_entries,
			     DLB_MAX_NUM_HIST_LIST_ENTRIES))
		return -1;

	if (dlb_bitmap_fill(hw->pf.avail_hist_list_entries))
		return -1;

	if (dlb_bitmap_alloc(hw,
			     &hw->pf.avail_qed_freelist_entries,
			     DLB_MAX_NUM_LDB_CREDITS))
		return -1;

	if (dlb_bitmap_fill(hw->pf.avail_qed_freelist_entries))
		return -1;

	if (dlb_bitmap_alloc(hw,
			     &hw->pf.avail_dqed_freelist_entries,
			     DLB_MAX_NUM_DIR_CREDITS))
		return -1;

	if (dlb_bitmap_fill(hw->pf.avail_dqed_freelist_entries))
		return -1;

	if (dlb_bitmap_alloc(hw,
			     &hw->pf.avail_aqed_freelist_entries,
			     DLB_MAX_NUM_AQOS_ENTRIES))
		return -1;

	if (dlb_bitmap_fill(hw->pf.avail_aqed_freelist_entries))
		return -1;

	/* Initialize the hardware resource IDs */
	for (i = 0; i < DLB_MAX_NUM_DOMAINS; i++)
		hw->domains[i].id = i;

	for (i = 0; i < DLB_MAX_NUM_LDB_QUEUES; i++)
		hw->rsrcs.ldb_queues[i].id = i;

	for (i = 0; i < DLB_MAX_NUM_LDB_PORTS; i++)
		hw->rsrcs.ldb_ports[i].id = i;

	for (i = 0; i < DLB_MAX_NUM_DIR_PORTS; i++)
		hw->rsrcs.dir_pq_pairs[i].id = i;

	for (i = 0; i < DLB_MAX_NUM_LDB_CREDIT_POOLS; i++)
		hw->rsrcs.ldb_credit_pools[i].id = i;

	for (i = 0; i < DLB_MAX_NUM_DIR_CREDIT_POOLS; i++)
		hw->rsrcs.dir_credit_pools[i].id = i;

	for (i = 0; i < DLB_MAX_NUM_SEQUENCE_NUMBER_GROUPS; i++) {
		hw->rsrcs.sn_groups[i].id = i;
		/* Default mode (0) is 32 sequence numbers per queue */
		hw->rsrcs.sn_groups[i].mode = 0;
		hw->rsrcs.sn_groups[i].sequence_numbers_per_queue = 32;
		hw->rsrcs.sn_groups[i].slot_use_bitmap = 0;
	}

	return 0;
}

void dlb_resource_free(struct dlb_hw *hw)
{
	dlb_bitmap_free(hw->pf.avail_hist_list_entries);

	dlb_bitmap_free(hw->pf.avail_qed_freelist_entries);

	dlb_bitmap_free(hw->pf.avail_dqed_freelist_entries);

	dlb_bitmap_free(hw->pf.avail_aqed_freelist_entries);
}

void dlb_hw_disable_vf_to_pf_isr_pend_err(struct dlb_hw *hw)
{
	union dlb_sys_sys_alarm_int_enable r0;

	r0.val = DLB_CSR_RD(hw, DLB_SYS_SYS_ALARM_INT_ENABLE);

	r0.field.vf_to_pf_isr_pend_error = 0;

	DLB_CSR_WR(hw, DLB_SYS_SYS_ALARM_INT_ENABLE, r0.val);
}
