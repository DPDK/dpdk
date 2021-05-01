/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#define DLB2_USE_NEW_HEADERS /* TEMPORARY FOR MERGE */

#include "dlb2_user.h"

#include "dlb2_hw_types_new.h"
#include "dlb2_osdep.h"
#include "dlb2_osdep_bitmap.h"
#include "dlb2_osdep_types.h"
#include "dlb2_regs_new.h"
#include "dlb2_resource_new.h" /* TEMP FOR UPSTREAMPATCHES */

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

/**
 * dlb2_resource_free() - free device state memory
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function frees software state pointed to by dlb2_hw. This function
 * should be called when resetting the device or unloading the driver.
 */
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

/**
 * dlb2_resource_init() - initialize the device
 * @hw: pointer to struct dlb2_hw.
 * @ver: device version.
 *
 * This function initializes the device's software state (pointed to by the hw
 * argument) and programs global scheduling QoS registers. This function should
 * be called during driver initialization, and the dlb2_hw structure should
 * be zero-initialized before calling the function.
 *
 * The dlb2_hw struct must be unique per DLB 2.0 device and persist until the
 * device is reset.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 */
int dlb2_resource_init(struct dlb2_hw *hw, enum dlb2_hw_ver ver)
{
	struct dlb2_list_entry *list;
	unsigned int i;
	int ret;

	/*
	 * For optimal load-balancing, ports that map to one or more QIDs in
	 * common should not be in numerical sequence. The port->QID mapping is
	 * application dependent, but the driver interleaves port IDs as much
	 * as possible to reduce the likelihood of sequential ports mapping to
	 * the same QID(s). This initial allocation of port IDs maximizes the
	 * average distance between an ID and its immediate neighbors (i.e.
	 * the distance from 1 to 0 and to 2, the distance from 2 to 1 and to
	 * 3, etc.).
	 */
	const u8 init_ldb_port_allocation[DLB2_MAX_NUM_LDB_PORTS] = {
		0,  7,  14,  5, 12,  3, 10,  1,  8, 15,  6, 13,  4, 11,  2,  9,
		16, 23, 30, 21, 28, 19, 26, 17, 24, 31, 22, 29, 20, 27, 18, 25,
		32, 39, 46, 37, 44, 35, 42, 33, 40, 47, 38, 45, 36, 43, 34, 41,
		48, 55, 62, 53, 60, 51, 58, 49, 56, 63, 54, 61, 52, 59, 50, 57,
	};

	hw->ver = ver;

	dlb2_init_fn_rsrc_lists(&hw->pf);

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++)
		dlb2_init_fn_rsrc_lists(&hw->vdev[i]);

	for (i = 0; i < DLB2_MAX_NUM_DOMAINS; i++) {
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

	hw->pf.num_avail_dir_pq_pairs = DLB2_MAX_NUM_DIR_PORTS(hw->ver);
	for (i = 0; i < hw->pf.num_avail_dir_pq_pairs; i++) {
		list = &hw->rsrcs.dir_pq_pairs[i].func_list;

		dlb2_list_add(&hw->pf.avail_dir_pq_pairs, list);
	}

	if (hw->ver == DLB2_HW_V2) {
		hw->pf.num_avail_qed_entries = DLB2_MAX_NUM_LDB_CREDITS;
		hw->pf.num_avail_dqed_entries =
			DLB2_MAX_NUM_DIR_CREDITS(hw->ver);
	} else {
		hw->pf.num_avail_entries = DLB2_MAX_NUM_CREDITS(hw->ver);
	}

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

	for (i = 0; i < DLB2_MAX_NUM_DIR_PORTS(hw->ver); i++) {
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

/**
 * dlb2_clr_pmcsr_disable() - power on bulk of DLB 2.0 logic
 * @hw: dlb2_hw handle for a particular device.
 * @ver: device version.
 *
 * Clearing the PMCSR must be done at initialization to make the device fully
 * operational.
 */
void dlb2_clr_pmcsr_disable(struct dlb2_hw *hw, enum dlb2_hw_ver ver)
{
	u32 pmcsr_dis;

	pmcsr_dis = DLB2_CSR_RD(hw, DLB2_CM_CFG_PM_PMCSR_DISABLE(ver));

	DLB2_BITS_CLR(pmcsr_dis, DLB2_CM_CFG_PM_PMCSR_DISABLE_DISABLE);

	DLB2_CSR_WR(hw, DLB2_CM_CFG_PM_PMCSR_DISABLE(ver), pmcsr_dis);
}

/**
 * dlb2_hw_get_num_resources() - query the PCI function's available resources
 * @hw: dlb2_hw handle for a particular device.
 * @arg: pointer to resource counts.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function returns the number of available resources for the PF or for a
 * VF.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, -EINVAL if vdev_req is true and vdev_id is
 * invalid.
 */
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

	if (hw->ver == DLB2_HW_V2) {
		arg->num_ldb_credits = rsrcs->num_avail_qed_entries;
		arg->num_dir_credits = rsrcs->num_avail_dqed_entries;
	} else {
		arg->num_credits = rsrcs->num_avail_entries;
	}
	return 0;
}
