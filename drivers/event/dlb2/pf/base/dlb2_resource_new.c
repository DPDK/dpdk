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

static void dlb2_configure_domain_credits_v2_5(struct dlb2_hw *hw,
					       struct dlb2_hw_domain *domain)
{
	u32 reg = 0;

	DLB2_BITS_SET(reg, domain->num_credits, DLB2_CHP_CFG_LDB_VAS_CRD_COUNT);
	DLB2_CSR_WR(hw, DLB2_CHP_CFG_VAS_CRD(domain->id.phys_id), reg);
}

static void dlb2_configure_domain_credits_v2(struct dlb2_hw *hw,
					     struct dlb2_hw_domain *domain)
{
	u32 reg = 0;

	DLB2_BITS_SET(reg, domain->num_ldb_credits,
		      DLB2_CHP_CFG_LDB_VAS_CRD_COUNT);
	DLB2_CSR_WR(hw, DLB2_CHP_CFG_LDB_VAS_CRD(domain->id.phys_id), reg);

	reg = 0;
	DLB2_BITS_SET(reg, domain->num_dir_credits,
		      DLB2_CHP_CFG_DIR_VAS_CRD_COUNT);
	DLB2_CSR_WR(hw, DLB2_CHP_CFG_DIR_VAS_CRD(domain->id.phys_id), reg);
}

static void dlb2_configure_domain_credits(struct dlb2_hw *hw,
					  struct dlb2_hw_domain *domain)
{
	if (hw->ver == DLB2_HW_V2)
		dlb2_configure_domain_credits_v2(hw, domain);
	else
		dlb2_configure_domain_credits_v2_5(hw, domain);
}

static int dlb2_attach_credits(struct dlb2_function_resources *rsrcs,
			       struct dlb2_hw_domain *domain,
			       u32 num_credits,
			       struct dlb2_cmd_response *resp)
{
	if (rsrcs->num_avail_entries < num_credits) {
		resp->status = DLB2_ST_CREDITS_UNAVAILABLE;
		return -EINVAL;
	}

	rsrcs->num_avail_entries -= num_credits;
	domain->num_credits += num_credits;
	return 0;
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

				if (ret)
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

		if (ret)
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
	if (ret)
		return ret;

	ret = dlb2_attach_ldb_ports(hw,
				    rsrcs,
				    domain,
				    args,
				    resp);
	if (ret)
		return ret;

	ret = dlb2_attach_dir_ports(hw,
				    rsrcs,
				    domain,
				    args->num_dir_ports,
				    resp);
	if (ret)
		return ret;

	if (hw->ver == DLB2_HW_V2) {
		ret = dlb2_attach_ldb_credits(rsrcs,
					      domain,
					      args->num_ldb_credits,
					      resp);
		if (ret)
			return ret;

		ret = dlb2_attach_dir_credits(rsrcs,
					      domain,
					      args->num_dir_credits,
					      resp);
		if (ret)
			return ret;
	} else {  /* DLB 2.5 */
		ret = dlb2_attach_credits(rsrcs,
					  domain,
					  args->num_credits,
					  resp);
		if (ret)
			return ret;
	}

	ret = dlb2_attach_domain_hist_list_entries(rsrcs,
						   domain,
						   args->num_hist_list_entries,
						   resp);
	if (ret)
		return ret;

	ret = dlb2_attach_atomic_inflights(rsrcs,
					   domain,
					   args->num_atomic_inflights,
					   resp);
	if (ret)
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
				  struct dlb2_cmd_response *resp,
				  struct dlb2_hw *hw,
				  struct dlb2_hw_domain **out_domain)
{
	u32 num_avail_ldb_ports, req_ldb_ports;
	struct dlb2_bitmap *avail_hl_entries;
	unsigned int max_contig_hl_range;
	struct dlb2_hw_domain *domain;
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

	domain = DLB2_FUNC_LIST_HEAD(rsrcs->avail_domains, typeof(*domain));
	if (domain == NULL) {
		resp->status = DLB2_ST_DOMAIN_UNAVAILABLE;
		return -EFAULT;
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
	if (hw->ver == DLB2_HW_V2_5) {
		if (rsrcs->num_avail_entries < args->num_credits) {
			resp->status = DLB2_ST_CREDITS_UNAVAILABLE;
			return -EINVAL;
		}
	} else {
		if (rsrcs->num_avail_qed_entries < args->num_ldb_credits) {
			resp->status = DLB2_ST_LDB_CREDITS_UNAVAILABLE;
			return -EINVAL;
		}
		if (rsrcs->num_avail_dqed_entries < args->num_dir_credits) {
			resp->status = DLB2_ST_DIR_CREDITS_UNAVAILABLE;
			return -EINVAL;
		}
	}

	if (rsrcs->num_avail_aqed_entries < args->num_atomic_inflights) {
		resp->status = DLB2_ST_ATOMIC_INFLIGHTS_UNAVAILABLE;
		return -EINVAL;
	}

	if (max_contig_hl_range < args->num_hist_list_entries) {
		resp->status = DLB2_ST_HIST_LIST_ENTRIES_UNAVAILABLE;
		return -EINVAL;
	}

	*out_domain = domain;

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
		    args->num_cos_ldb_ports[2]);
	DLB2_HW_DBG(hw, "\tNumber of LDB ports (CoS 3):   %d\n",
		    args->num_cos_ldb_ports[3]);
	DLB2_HW_DBG(hw, "\tStrict CoS allocation:         %d\n",
		    args->cos_strict);
	DLB2_HW_DBG(hw, "\tNumber of DIR ports:           %d\n",
		    args->num_dir_ports);
	DLB2_HW_DBG(hw, "\tNumber of ATM inflights:       %d\n",
		    args->num_atomic_inflights);
	DLB2_HW_DBG(hw, "\tNumber of hist list entries:   %d\n",
		    args->num_hist_list_entries);
	if (hw->ver == DLB2_HW_V2) {
		DLB2_HW_DBG(hw, "\tNumber of LDB credits:         %d\n",
			    args->num_ldb_credits);
		DLB2_HW_DBG(hw, "\tNumber of DIR credits:         %d\n",
			    args->num_dir_credits);
	} else {
		DLB2_HW_DBG(hw, "\tNumber of credits:         %d\n",
			    args->num_credits);
	}
}

/**
 * dlb2_hw_create_sched_domain() - create a scheduling domain
 * @hw: dlb2_hw handle for a particular device.
 * @args: scheduling domain creation arguments.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function creates a scheduling domain containing the resources specified
 * in args. The individual resources (queues, ports, credits) can be configured
 * after creating a scheduling domain.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the domain ID.
 *
 * resp->id contains a virtual ID if vdev_req is true.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, or the requested domain name
 *	    is already in use.
 * EFAULT - Internal error (resp->status not set).
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
	ret = dlb2_verify_create_sched_dom_args(rsrcs, args, resp, hw, &domain);
	if (ret)
		return ret;

	dlb2_init_domain_rsrc_lists(domain);

	ret = dlb2_domain_attach_resources(hw, rsrcs, domain, args, resp);
	if (ret) {
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
