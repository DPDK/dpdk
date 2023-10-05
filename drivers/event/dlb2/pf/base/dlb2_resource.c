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

/*
 * The PF driver cannot assume that a register write will affect subsequent HCW
 * writes. To ensure a write completes, the driver must read back a CSR. This
 * function only need be called for configuration that can occur after the
 * domain has started; prior to starting, applications can't send HCWs.
 */
static inline void dlb2_flush_csr(struct dlb2_hw *hw)
{
	DLB2_CSR_RD(hw, DLB2_SYS_TOTAL_VAS(hw->ver));
}

static void dlb2_init_domain_rsrc_lists(struct dlb2_hw_domain *domain)
{
	int i;

	dlb2_list_init_head(&domain->used_ldb_queues);
	dlb2_list_init_head(&domain->used_dir_pq_pairs);
	dlb2_list_init_head(&domain->avail_ldb_queues);
	dlb2_list_init_head(&domain->avail_dir_pq_pairs);
	dlb2_list_init_head(&domain->rsvd_dir_pq_pairs);

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
int dlb2_resource_init(struct dlb2_hw *hw, enum dlb2_hw_ver ver, const void *probe_args)
{
	const struct dlb2_devargs *args = (const struct dlb2_devargs *)probe_args;
	bool ldb_port_default = args ? args->default_ldb_port_allocation : false;
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

		if (ldb_port_default == true)
			port = &hw->rsrcs.ldb_ports[init_ldb_port_allocation[i]];
		else
			port = &hw->rsrcs.ldb_ports[hw->ldb_pp_allocations[i]];

		dlb2_list_add(&hw->pf.avail_ldb_ports[cos_id],
			      &port->func_list);
	}

	hw->pf.num_avail_dir_pq_pairs = DLB2_MAX_NUM_DIR_PORTS(hw->ver);
	for (i = 0; i < hw->pf.num_avail_dir_pq_pairs; i++) {
		int index = hw->dir_pp_allocations[i];
		list = &hw->rsrcs.dir_pq_pairs[index].func_list;

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
			/* Allocate from best performing cos */
			u32 cos_idx = j + DLB2_MAX_NUM_LDB_PORTS;
			u32 cos_id = hw->ldb_pp_allocations[cos_idx];
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

	return 0;
}

static int dlb2_attach_dir_ports(struct dlb2_hw *hw,
				 struct dlb2_function_resources *rsrcs,
				 struct dlb2_hw_domain *domain,
				 u32 num_ports,
				 struct dlb2_cmd_response *resp)
{
	int num_res = hw->num_prod_cores;
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

		if (num_res) {
			dlb2_list_add(&domain->rsvd_dir_pq_pairs,
				      &port->domain_list);
			num_res--;
		} else {
			dlb2_list_add(&domain->avail_dir_pq_pairs,
			&port->domain_list);
		}

		dlb2_list_del(&rsrcs->avail_dir_pq_pairs, &port->func_list);

		port->domain_id = domain->id;
		port->owned = true;
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
dlb2_pp_profile(struct dlb2_hw *hw, int port, bool is_ldb)
{
	u64 cycle_start = 0ULL, cycle_end = 0ULL;
	struct dlb2_hcw hcw_mem[DLB2_HCW_MEM_SIZE], *hcw;
	void __iomem *pp_addr;
	int i;

	pp_addr = os_map_producer_port(hw, port, is_ldb);

	/* Point hcw to a 64B-aligned location */
	hcw = (struct dlb2_hcw *)((uintptr_t)&hcw_mem[DLB2_HCW_64B_OFF] &
	      ~DLB2_HCW_ALIGN_MASK);

	/*
	 * Program the first HCW for a completion and token return and
	 * the other HCWs as NOOPS
	 */

	memset(hcw, 0, (DLB2_HCW_MEM_SIZE - DLB2_HCW_64B_OFF) * sizeof(*hcw));
	hcw->qe_comp = 1;
	hcw->cq_token = 1;
	hcw->lock_id = 1;

	cycle_start = rte_get_tsc_cycles();
	for (i = 0; i < DLB2_NUM_PROBE_ENQS; i++)
		dlb2_movdir64b(pp_addr, hcw);

	cycle_end = rte_get_tsc_cycles();

	os_unmap_producer_port(hw, pp_addr);
	return (int)(cycle_end - cycle_start);
}

static uint32_t
dlb2_pp_profile_func(void *data)
{
	struct dlb2_pp_thread_data *thread_data = data;

	thread_data->cycles = dlb2_pp_profile(thread_data->hw,
			thread_data->pp, thread_data->is_ldb);

	return 0;
}

static int dlb2_pp_cycle_comp(const void *a, const void *b)
{
	const struct dlb2_pp_thread_data *x = a;
	const struct dlb2_pp_thread_data *y = b;

	return x->cycles - y->cycles;
}


/* Probe producer ports from different CPU cores */
static void
dlb2_get_pp_allocation(struct dlb2_hw *hw, int cpu, int port_type)
{
	struct dlb2_pp_thread_data dlb2_thread_data[DLB2_MAX_NUM_DIR_PORTS_V2_5];
	struct dlb2_dev *dlb2_dev = container_of(hw, struct dlb2_dev, hw);
	struct dlb2_pp_thread_data cos_cycles[DLB2_NUM_COS_DOMAINS];
	int ver = DLB2_HW_DEVICE_FROM_PCI_ID(dlb2_dev->pdev);
	int num_ports_per_sort, num_ports, num_sort, i, err;
	bool is_ldb = (port_type == DLB2_LDB_PORT);
	int *port_allocations;
	rte_thread_t thread;
	rte_thread_attr_t th_attr;
	char th_name[RTE_THREAD_INTERNAL_NAME_SIZE];

	if (is_ldb) {
		port_allocations = hw->ldb_pp_allocations;
		num_ports = DLB2_MAX_NUM_LDB_PORTS;
		num_sort = DLB2_NUM_COS_DOMAINS;
	} else {
		port_allocations = hw->dir_pp_allocations;
		num_ports = DLB2_MAX_NUM_DIR_PORTS(ver);
		num_sort = 1;
	}

	num_ports_per_sort = num_ports / num_sort;

	dlb2_dev->enqueue_four = dlb2_movdir64b;

	DLB2_LOG_INFO(" for %s: cpu core used in pp profiling: %d\n",
		      is_ldb ? "LDB" : "DIR", cpu);

	memset(cos_cycles, 0, num_sort * sizeof(struct dlb2_pp_thread_data));
	for (i = 0; i < num_ports; i++) {
		int cos = (i >> DLB2_NUM_COS_DOMAINS) % DLB2_NUM_COS_DOMAINS;
		dlb2_thread_data[i].is_ldb = is_ldb;
		dlb2_thread_data[i].pp = i;
		dlb2_thread_data[i].cycles = 0;
		dlb2_thread_data[i].hw = hw;

		err = rte_thread_attr_init(&th_attr);
		if (err != 0) {
			DLB2_LOG_ERR(": thread attribute failed! err=%d", err);
			return;
		}
		CPU_SET(cpu, &th_attr.cpuset);

		err = rte_thread_create(&thread, &th_attr,
				&dlb2_pp_profile_func, &dlb2_thread_data[i]);
		if (err) {
			DLB2_LOG_ERR(": thread creation failed! err=%d", err);
			return;
		}

		snprintf(th_name, sizeof(th_name), "dlb2-pp%d", cpu);
		rte_thread_set_prefixed_name(thread, th_name);

		err = rte_thread_join(thread, NULL);
		if (err) {
			DLB2_LOG_ERR(": thread join failed! err=%d", err);
			return;
		}

		if (is_ldb)
			cos_cycles[cos].cycles += dlb2_thread_data[i].cycles;

		if ((i + 1) % num_ports_per_sort == 0) {
			int index = 0;

			if (is_ldb) {
				cos_cycles[cos].pp = cos;
				index = cos * num_ports_per_sort;
			}
			/*
			 * For LDB ports first sort with in a cos. Later sort
			 * the best cos based on total cycles for the cos.
			 * For DIR ports, there is a single sort across all
			 * ports.
			 */
			qsort(&dlb2_thread_data[index], num_ports_per_sort,
			      sizeof(struct dlb2_pp_thread_data),
			      dlb2_pp_cycle_comp);
		}
	}

	/*
	 * Sort by best cos aggregated over all ports per cos
	 * Note: After DLB2_MAX_NUM_LDB_PORTS sorted cos is stored and so'pp'
	 * is cos_id and not port id.
	 */
	if (is_ldb) {
		qsort(cos_cycles, num_sort, sizeof(struct dlb2_pp_thread_data),
		      dlb2_pp_cycle_comp);
		for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
			port_allocations[i + DLB2_MAX_NUM_LDB_PORTS] = cos_cycles[i].pp;
	}

	for (i = 0; i < num_ports; i++) {
		port_allocations[i] = dlb2_thread_data[i].pp;
		DLB2_LOG_INFO(": pp %d cycles %d", port_allocations[i],
			      dlb2_thread_data[i].cycles);
	}

}

int
dlb2_resource_probe(struct dlb2_hw *hw, const void *probe_args)
{
	const struct dlb2_devargs *args = (const struct dlb2_devargs *)probe_args;
	const char *mask = args ? args->producer_coremask : NULL;
	int cpu = 0, cnt = 0, cores[RTE_MAX_LCORE], i;

	if (args) {
		mask = (const char *)args->producer_coremask;
	}

	if (mask && rte_eal_parse_coremask(mask, cores)) {
		DLB2_LOG_ERR(": Invalid producer coremask=%s", mask);
		return -1;
	}

	hw->num_prod_cores = 0;
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		bool is_pcore = (mask && cores[i] != -1);

		if (rte_lcore_is_enabled(i)) {
			if (is_pcore) {
				/*
				 * Populate the producer cores from parsed
				 * coremask
				 */
				hw->prod_core_list[cores[i]] = i;
				hw->num_prod_cores++;

			} else if ((++cnt == DLB2_EAL_PROBE_CORE ||
			   rte_lcore_count() < DLB2_EAL_PROBE_CORE)) {
				/*
				 * If no producer coremask is provided, use the
				 * second EAL core to probe
				 */
				cpu = i;
				break;
			}
		} else if (is_pcore) {
			DLB2_LOG_ERR("Producer coremask(%s) must be a subset of EAL coremask",
				     mask);
			return -1;
		}

	}
	/* Use the first core in producer coremask to probe */
	if (hw->num_prod_cores)
		cpu = hw->prod_core_list[0];

	dlb2_get_pp_allocation(hw, cpu, DLB2_LDB_PORT);
	dlb2_get_pp_allocation(hw, cpu, DLB2_DIR_PORT);

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

static void dlb2_dir_port_cq_disable(struct dlb2_hw *hw,
				     struct dlb2_dir_pq_pair *port)
{
	u32 reg = 0;

	DLB2_BIT_SET(reg, DLB2_LSP_CQ_DIR_DSBL_DISABLED);
	DLB2_CSR_WR(hw, DLB2_LSP_CQ_DIR_DSBL(hw->ver, port->id.phys_id), reg);

	dlb2_flush_csr(hw);
}

static u32 dlb2_dir_cq_token_count(struct dlb2_hw *hw,
				   struct dlb2_dir_pq_pair *port)
{
	u32 cnt;

	cnt = DLB2_CSR_RD(hw,
			  DLB2_LSP_CQ_DIR_TKN_CNT(hw->ver, port->id.phys_id));

	/*
	 * Account for the initial token count, which is used in order to
	 * provide a CQ with depth less than 8.
	 */

	return DLB2_BITS_GET(cnt, DLB2_LSP_CQ_DIR_TKN_CNT_COUNT) -
	       port->init_tkn_cnt;
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
		void __iomem *pp_addr;

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

	return cnt;
}

static void dlb2_dir_port_cq_enable(struct dlb2_hw *hw,
				    struct dlb2_dir_pq_pair *port)
{
	u32 reg = 0;

	DLB2_CSR_WR(hw, DLB2_LSP_CQ_DIR_DSBL(hw->ver, port->id.phys_id), reg);

	dlb2_flush_csr(hw);
}

static int dlb2_domain_drain_dir_cqs(struct dlb2_hw *hw,
				     struct dlb2_hw_domain *domain,
				     bool toggle_port)
{
	struct dlb2_list_entry *iter;
	struct dlb2_dir_pq_pair *port;
	int drain_cnt = 0;
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

		drain_cnt = dlb2_drain_dir_cq(hw, port);

		if (toggle_port)
			dlb2_dir_port_cq_enable(hw, port);
	}

	return drain_cnt;
}

static u32 dlb2_dir_queue_depth(struct dlb2_hw *hw,
				struct dlb2_dir_pq_pair *queue)
{
	u32 cnt;

	cnt = DLB2_CSR_RD(hw, DLB2_LSP_QID_DIR_ENQUEUE_CNT(hw->ver,
						      queue->id.phys_id));

	return DLB2_BITS_GET(cnt, DLB2_LSP_QID_DIR_ENQUEUE_CNT_COUNT);
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
	int i;

	/* If the domain hasn't been started, there's no traffic to drain */
	if (!domain->started)
		return 0;

	for (i = 0; i < DLB2_MAX_QID_EMPTY_CHECK_LOOPS; i++) {
		int drain_cnt;

		drain_cnt = dlb2_domain_drain_dir_cqs(hw, domain, false);

		if (dlb2_domain_dir_queues_empty(hw, domain))
			break;

		/*
		 * Allow time for DLB to schedule QEs before draining
		 * the CQs again.
		 */
		if (!drain_cnt)
			rte_delay_us(1);

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
	dlb2_domain_drain_dir_cqs(hw, domain, true);

	return 0;
}

static void dlb2_ldb_port_cq_enable(struct dlb2_hw *hw,
				    struct dlb2_ldb_port *port)
{
	u32 reg = 0;

	/*
	 * Don't re-enable the port if a removal is pending. The caller should
	 * mark this port as enabled (if it isn't already), and when the
	 * removal completes the port will be enabled.
	 */
	if (port->num_pending_removals)
		return;

	DLB2_CSR_WR(hw, DLB2_LSP_CQ_LDB_DSBL(hw->ver, port->id.phys_id), reg);

	dlb2_flush_csr(hw);
}

static void dlb2_ldb_port_cq_disable(struct dlb2_hw *hw,
				     struct dlb2_ldb_port *port)
{
	u32 reg = 0;

	DLB2_BIT_SET(reg, DLB2_LSP_CQ_LDB_DSBL_DISABLED);
	DLB2_CSR_WR(hw, DLB2_LSP_CQ_LDB_DSBL(hw->ver, port->id.phys_id), reg);

	dlb2_flush_csr(hw);
}

static u32 dlb2_ldb_cq_inflight_count(struct dlb2_hw *hw,
				      struct dlb2_ldb_port *port)
{
	u32 cnt;

	cnt = DLB2_CSR_RD(hw,
			  DLB2_LSP_CQ_LDB_INFL_CNT(hw->ver, port->id.phys_id));

	return DLB2_BITS_GET(cnt, DLB2_LSP_CQ_LDB_INFL_CNT_COUNT);
}

static u32 dlb2_ldb_cq_token_count(struct dlb2_hw *hw,
				   struct dlb2_ldb_port *port)
{
	u32 cnt;

	cnt = DLB2_CSR_RD(hw,
			  DLB2_LSP_CQ_LDB_TKN_CNT(hw->ver, port->id.phys_id));

	/*
	 * Account for the initial token count, which is used in order to
	 * provide a CQ with depth less than 8.
	 */

	return DLB2_BITS_GET(cnt, DLB2_LSP_CQ_LDB_TKN_CNT_TOKEN_COUNT) -
		port->init_tkn_cnt;
}

static int dlb2_drain_ldb_cq(struct dlb2_hw *hw, struct dlb2_ldb_port *port)
{
	u32 infl_cnt, tkn_cnt;
	unsigned int i;

	infl_cnt = dlb2_ldb_cq_inflight_count(hw, port);
	tkn_cnt = dlb2_ldb_cq_token_count(hw, port);

	if (infl_cnt || tkn_cnt) {
		struct dlb2_hcw hcw_mem[8], *hcw;
		void __iomem *pp_addr;

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

	return tkn_cnt;
}

static int dlb2_domain_drain_ldb_cqs(struct dlb2_hw *hw,
				      struct dlb2_hw_domain *domain,
				      bool toggle_port)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_port *port;
	int drain_cnt = 0;
	int i;
	RTE_SET_USED(iter);

	/* If the domain hasn't been started, there's no traffic to drain */
	if (!domain->started)
		return 0;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			if (toggle_port)
				dlb2_ldb_port_cq_disable(hw, port);

			drain_cnt = dlb2_drain_ldb_cq(hw, port);

			if (toggle_port)
				dlb2_ldb_port_cq_enable(hw, port);
		}
	}

	return drain_cnt;
}

static u32 dlb2_ldb_queue_depth(struct dlb2_hw *hw,
				struct dlb2_ldb_queue *queue)
{
	u32 aqed, ldb, atm;

	aqed = DLB2_CSR_RD(hw, DLB2_LSP_QID_AQED_ACTIVE_CNT(hw->ver,
						       queue->id.phys_id));
	ldb = DLB2_CSR_RD(hw, DLB2_LSP_QID_LDB_ENQUEUE_CNT(hw->ver,
						      queue->id.phys_id));
	atm = DLB2_CSR_RD(hw,
			  DLB2_LSP_QID_ATM_ACTIVE(hw->ver, queue->id.phys_id));

	return DLB2_BITS_GET(aqed, DLB2_LSP_QID_AQED_ACTIVE_CNT_COUNT)
	       + DLB2_BITS_GET(ldb, DLB2_LSP_QID_LDB_ENQUEUE_CNT_COUNT)
	       + DLB2_BITS_GET(atm, DLB2_LSP_QID_ATM_ACTIVE_COUNT);
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
	int i;

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
		int drain_cnt;

		drain_cnt = dlb2_domain_drain_ldb_cqs(hw, domain, false);

		if (dlb2_domain_mapped_queues_empty(hw, domain))
			break;

		/*
		 * Allow time for DLB to schedule QEs before draining
		 * the CQs again.
		 */
		if (!drain_cnt)
			rte_delay_us(1);
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
	dlb2_domain_drain_ldb_cqs(hw, domain, true);

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
		DLB2_DOM_LIST_FOR(domain->used_ldb_queues, queue, iter2) {
			if (queue->id.virt_id == id)
				return queue;
		}
	}

	DLB2_FUNC_LIST_FOR(rsrcs->avail_ldb_queues, queue, iter1) {
		if (queue->id.virt_id == id)
			return queue;
	}

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

	DLB2_FUNC_LIST_FOR(rsrcs->used_domains, domain, iteration) {
		if (domain->id.virt_id == id)
			return domain;
	}

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
	u32 ctrl = 0;

	DLB2_BITS_SET(ctrl, port->id.phys_id, DLB2_LSP_LDB_SCHED_CTRL_CQ);
	DLB2_BITS_SET(ctrl, slot, DLB2_LSP_LDB_SCHED_CTRL_QIDIX);
	DLB2_BIT_SET(ctrl, DLB2_LSP_LDB_SCHED_CTRL_INFLIGHT_OK_V);

	DLB2_CSR_WR(hw, DLB2_LSP_LDB_SCHED_CTRL(hw->ver), ctrl);

	dlb2_flush_csr(hw);
}

static void dlb2_ldb_port_set_queue_if_status(struct dlb2_hw *hw,
					      struct dlb2_ldb_port *port,
					      int slot)
{
	u32 ctrl = 0;

	DLB2_BITS_SET(ctrl, port->id.phys_id, DLB2_LSP_LDB_SCHED_CTRL_CQ);
	DLB2_BITS_SET(ctrl, slot, DLB2_LSP_LDB_SCHED_CTRL_QIDIX);
	DLB2_BIT_SET(ctrl, DLB2_LSP_LDB_SCHED_CTRL_VALUE);
	DLB2_BIT_SET(ctrl, DLB2_LSP_LDB_SCHED_CTRL_INFLIGHT_OK_V);

	DLB2_CSR_WR(hw, DLB2_LSP_LDB_SCHED_CTRL(hw->ver), ctrl);

	dlb2_flush_csr(hw);
}

static int dlb2_ldb_port_map_qid_static(struct dlb2_hw *hw,
					struct dlb2_ldb_port *p,
					struct dlb2_ldb_queue *q,
					u8 priority)
{
	enum dlb2_qid_map_state state;
	u32 lsp_qid2cq2;
	u32 lsp_qid2cq;
	u32 atm_qid2cq;
	u32 cq2priov;
	u32 cq2qid;
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

	/* Read-modify-write the priority and valid bit register */
	cq2priov = DLB2_CSR_RD(hw, DLB2_LSP_CQ2PRIOV(hw->ver, p->id.phys_id));

	cq2priov |= (1 << (i + DLB2_LSP_CQ2PRIOV_V_LOC)) & DLB2_LSP_CQ2PRIOV_V;
	cq2priov |= ((priority & 0x7) << (i + DLB2_LSP_CQ2PRIOV_PRIO_LOC) * 3)
		    & DLB2_LSP_CQ2PRIOV_PRIO;

	DLB2_CSR_WR(hw, DLB2_LSP_CQ2PRIOV(hw->ver, p->id.phys_id), cq2priov);

	/* Read-modify-write the QID map register */
	if (i < 4)
		cq2qid = DLB2_CSR_RD(hw, DLB2_LSP_CQ2QID0(hw->ver,
							  p->id.phys_id));
	else
		cq2qid = DLB2_CSR_RD(hw, DLB2_LSP_CQ2QID1(hw->ver,
							  p->id.phys_id));

	if (i == 0 || i == 4)
		DLB2_BITS_SET(cq2qid, q->id.phys_id, DLB2_LSP_CQ2QID0_QID_P0);
	if (i == 1 || i == 5)
		DLB2_BITS_SET(cq2qid, q->id.phys_id, DLB2_LSP_CQ2QID0_QID_P1);
	if (i == 2 || i == 6)
		DLB2_BITS_SET(cq2qid, q->id.phys_id, DLB2_LSP_CQ2QID0_QID_P2);
	if (i == 3 || i == 7)
		DLB2_BITS_SET(cq2qid, q->id.phys_id, DLB2_LSP_CQ2QID0_QID_P3);

	if (i < 4)
		DLB2_CSR_WR(hw,
			    DLB2_LSP_CQ2QID0(hw->ver, p->id.phys_id), cq2qid);
	else
		DLB2_CSR_WR(hw,
			    DLB2_LSP_CQ2QID1(hw->ver, p->id.phys_id), cq2qid);

	atm_qid2cq = DLB2_CSR_RD(hw,
				 DLB2_ATM_QID2CQIDIX(q->id.phys_id,
						p->id.phys_id / 4));

	lsp_qid2cq = DLB2_CSR_RD(hw,
				 DLB2_LSP_QID2CQIDIX(hw->ver, q->id.phys_id,
						p->id.phys_id / 4));

	lsp_qid2cq2 = DLB2_CSR_RD(hw,
				  DLB2_LSP_QID2CQIDIX2(hw->ver, q->id.phys_id,
						  p->id.phys_id / 4));

	switch (p->id.phys_id % 4) {
	case 0:
		DLB2_BIT_SET(atm_qid2cq,
			     1 << (i + DLB2_ATM_QID2CQIDIX_00_CQ_P0_LOC));
		DLB2_BIT_SET(lsp_qid2cq,
			     1 << (i + DLB2_LSP_QID2CQIDIX_00_CQ_P0_LOC));
		DLB2_BIT_SET(lsp_qid2cq2,
			     1 << (i + DLB2_LSP_QID2CQIDIX2_00_CQ_P0_LOC));
		break;

	case 1:
		DLB2_BIT_SET(atm_qid2cq,
			     1 << (i + DLB2_ATM_QID2CQIDIX_00_CQ_P1_LOC));
		DLB2_BIT_SET(lsp_qid2cq,
			     1 << (i + DLB2_LSP_QID2CQIDIX_00_CQ_P1_LOC));
		DLB2_BIT_SET(lsp_qid2cq2,
			     1 << (i + DLB2_LSP_QID2CQIDIX2_00_CQ_P1_LOC));
		break;

	case 2:
		DLB2_BIT_SET(atm_qid2cq,
			     1 << (i + DLB2_ATM_QID2CQIDIX_00_CQ_P2_LOC));
		DLB2_BIT_SET(lsp_qid2cq,
			     1 << (i + DLB2_LSP_QID2CQIDIX_00_CQ_P2_LOC));
		DLB2_BIT_SET(lsp_qid2cq2,
			     1 << (i + DLB2_LSP_QID2CQIDIX2_00_CQ_P2_LOC));
		break;

	case 3:
		DLB2_BIT_SET(atm_qid2cq,
			     1 << (i + DLB2_ATM_QID2CQIDIX_00_CQ_P3_LOC));
		DLB2_BIT_SET(lsp_qid2cq,
			     1 << (i + DLB2_LSP_QID2CQIDIX_00_CQ_P3_LOC));
		DLB2_BIT_SET(lsp_qid2cq2,
			     1 << (i + DLB2_LSP_QID2CQIDIX2_00_CQ_P3_LOC));
		break;
	}

	DLB2_CSR_WR(hw,
		    DLB2_ATM_QID2CQIDIX(q->id.phys_id, p->id.phys_id / 4),
		    atm_qid2cq);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_QID2CQIDIX(hw->ver,
					q->id.phys_id, p->id.phys_id / 4),
		    lsp_qid2cq);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_QID2CQIDIX2(hw->ver,
					 q->id.phys_id, p->id.phys_id / 4),
		    lsp_qid2cq2);

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
	u32 ctrl = 0;
	u32 active;
	u32 enq;

	/* Set the atomic scheduling haswork bit */
	active = DLB2_CSR_RD(hw, DLB2_LSP_QID_AQED_ACTIVE_CNT(hw->ver,
							 queue->id.phys_id));

	DLB2_BITS_SET(ctrl, port->id.phys_id, DLB2_LSP_LDB_SCHED_CTRL_CQ);
	DLB2_BITS_SET(ctrl, slot, DLB2_LSP_LDB_SCHED_CTRL_QIDIX);
	DLB2_BIT_SET(ctrl, DLB2_LSP_LDB_SCHED_CTRL_VALUE);
	DLB2_BITS_SET(ctrl,
		      DLB2_BITS_GET(active,
				    DLB2_LSP_QID_AQED_ACTIVE_CNT_COUNT) > 0,
				    DLB2_LSP_LDB_SCHED_CTRL_RLIST_HASWORK_V);

	/* Set the non-atomic scheduling haswork bit */
	DLB2_CSR_WR(hw, DLB2_LSP_LDB_SCHED_CTRL(hw->ver), ctrl);

	enq = DLB2_CSR_RD(hw,
			  DLB2_LSP_QID_LDB_ENQUEUE_CNT(hw->ver,
						       queue->id.phys_id));

	memset(&ctrl, 0, sizeof(ctrl));

	DLB2_BITS_SET(ctrl, port->id.phys_id, DLB2_LSP_LDB_SCHED_CTRL_CQ);
	DLB2_BITS_SET(ctrl, slot, DLB2_LSP_LDB_SCHED_CTRL_QIDIX);
	DLB2_BIT_SET(ctrl, DLB2_LSP_LDB_SCHED_CTRL_VALUE);
	DLB2_BITS_SET(ctrl,
		      DLB2_BITS_GET(enq,
				    DLB2_LSP_QID_LDB_ENQUEUE_CNT_COUNT) > 0,
		      DLB2_LSP_LDB_SCHED_CTRL_NALB_HASWORK_V);

	DLB2_CSR_WR(hw, DLB2_LSP_LDB_SCHED_CTRL(hw->ver), ctrl);

	dlb2_flush_csr(hw);

	return 0;
}

static void dlb2_ldb_port_clear_has_work_bits(struct dlb2_hw *hw,
					      struct dlb2_ldb_port *port,
					      u8 slot)
{
	u32 ctrl = 0;

	DLB2_BITS_SET(ctrl, port->id.phys_id, DLB2_LSP_LDB_SCHED_CTRL_CQ);
	DLB2_BITS_SET(ctrl, slot, DLB2_LSP_LDB_SCHED_CTRL_QIDIX);
	DLB2_BIT_SET(ctrl, DLB2_LSP_LDB_SCHED_CTRL_RLIST_HASWORK_V);

	DLB2_CSR_WR(hw, DLB2_LSP_LDB_SCHED_CTRL(hw->ver), ctrl);

	memset(&ctrl, 0, sizeof(ctrl));

	DLB2_BITS_SET(ctrl, port->id.phys_id, DLB2_LSP_LDB_SCHED_CTRL_CQ);
	DLB2_BITS_SET(ctrl, slot, DLB2_LSP_LDB_SCHED_CTRL_QIDIX);
	DLB2_BIT_SET(ctrl, DLB2_LSP_LDB_SCHED_CTRL_NALB_HASWORK_V);

	DLB2_CSR_WR(hw, DLB2_LSP_LDB_SCHED_CTRL(hw->ver), ctrl);

	dlb2_flush_csr(hw);
}


static void dlb2_ldb_queue_set_inflight_limit(struct dlb2_hw *hw,
					      struct dlb2_ldb_queue *queue)
{
	u32 infl_lim = 0;

	DLB2_BITS_SET(infl_lim, queue->num_qid_inflights,
		 DLB2_LSP_QID_LDB_INFL_LIM_LIMIT);

	DLB2_CSR_WR(hw, DLB2_LSP_QID_LDB_INFL_LIM(hw->ver, queue->id.phys_id),
		    infl_lim);
}

static void dlb2_ldb_queue_clear_inflight_limit(struct dlb2_hw *hw,
						struct dlb2_ldb_queue *queue)
{
	DLB2_CSR_WR(hw,
		    DLB2_LSP_QID_LDB_INFL_LIM(hw->ver, queue->id.phys_id),
		    DLB2_LSP_QID_LDB_INFL_LIM_RST);
}

static int dlb2_ldb_port_finish_map_qid_dynamic(struct dlb2_hw *hw,
						struct dlb2_hw_domain *domain,
						struct dlb2_ldb_port *port,
						struct dlb2_ldb_queue *queue)
{
	struct dlb2_list_entry *iter;
	enum dlb2_qid_map_state state;
	int slot, ret, i;
	u32 infl_cnt;
	u8 prio;
	RTE_SET_USED(iter);

	infl_cnt = DLB2_CSR_RD(hw,
			       DLB2_LSP_QID_LDB_INFL_CNT(hw->ver,
						    queue->id.phys_id));

	if (DLB2_BITS_GET(infl_cnt, DLB2_LSP_QID_LDB_INFL_CNT_COUNT)) {
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
	enum dlb2_qid_map_state state;
	struct dlb2_hw_domain *domain;
	int domain_id, slot, ret;
	u32 infl_cnt;

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
	DLB2_CSR_WR(hw, DLB2_LSP_QID_LDB_INFL_LIM(hw->ver,
						  queue->id.phys_id), 0);

	if (!dlb2_port_find_slot(port, DLB2_QUEUE_UNMAPPED, &slot)) {
		DLB2_HW_ERR(hw,
			    "Internal error: No available unmapped slots\n");
		return -EFAULT;
	}

	port->qid_map[slot].qid = queue->id.phys_id;
	port->qid_map[slot].priority = priority;

	state = DLB2_QUEUE_MAP_IN_PROG;
	ret = dlb2_port_slot_state_transition(hw, port, queue, slot, state);
	if (ret)
		return ret;

	infl_cnt = DLB2_CSR_RD(hw,
			       DLB2_LSP_QID_LDB_INFL_CNT(hw->ver,
						    queue->id.phys_id));

	if (DLB2_BITS_GET(infl_cnt, DLB2_LSP_QID_LDB_INFL_CNT_COUNT)) {
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

	infl_cnt = DLB2_CSR_RD(hw,
			       DLB2_LSP_QID_LDB_INFL_CNT(hw->ver,
						    queue->id.phys_id));

	if (DLB2_BITS_GET(infl_cnt, DLB2_LSP_QID_LDB_INFL_CNT_COUNT)) {
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
		u32 infl_cnt;
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

		infl_cnt = DLB2_CSR_RD(hw,
				       DLB2_LSP_QID_LDB_INFL_CNT(hw->ver, qid));

		if (DLB2_BITS_GET(infl_cnt, DLB2_LSP_QID_LDB_INFL_CNT_COUNT))
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

		infl_cnt = DLB2_CSR_RD(hw,
				       DLB2_LSP_QID_LDB_INFL_CNT(hw->ver, qid));

		if (DLB2_BITS_GET(infl_cnt, DLB2_LSP_QID_LDB_INFL_CNT_COUNT)) {
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
	u32 lsp_qid2cq2;
	u32 lsp_qid2cq;
	u32 atm_qid2cq;
	u32 cq2priov;
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

	port_id = port->id.phys_id;
	queue_id = queue->id.phys_id;

	/* Read-modify-write the priority and valid bit register */
	cq2priov = DLB2_CSR_RD(hw, DLB2_LSP_CQ2PRIOV(hw->ver, port_id));

	cq2priov &= ~(1 << (i + DLB2_LSP_CQ2PRIOV_V_LOC));

	DLB2_CSR_WR(hw, DLB2_LSP_CQ2PRIOV(hw->ver, port_id), cq2priov);

	atm_qid2cq = DLB2_CSR_RD(hw, DLB2_ATM_QID2CQIDIX(queue_id,
							 port_id / 4));

	lsp_qid2cq = DLB2_CSR_RD(hw,
				 DLB2_LSP_QID2CQIDIX(hw->ver,
						queue_id, port_id / 4));

	lsp_qid2cq2 = DLB2_CSR_RD(hw,
				  DLB2_LSP_QID2CQIDIX2(hw->ver,
						  queue_id, port_id / 4));

	switch (port_id % 4) {
	case 0:
		atm_qid2cq &= ~(1 << (i + DLB2_ATM_QID2CQIDIX_00_CQ_P0_LOC));
		lsp_qid2cq &= ~(1 << (i + DLB2_LSP_QID2CQIDIX_00_CQ_P0_LOC));
		lsp_qid2cq2 &= ~(1 << (i + DLB2_LSP_QID2CQIDIX2_00_CQ_P0_LOC));
		break;

	case 1:
		atm_qid2cq &= ~(1 << (i + DLB2_ATM_QID2CQIDIX_00_CQ_P1_LOC));
		lsp_qid2cq &= ~(1 << (i + DLB2_LSP_QID2CQIDIX_00_CQ_P1_LOC));
		lsp_qid2cq2 &= ~(1 << (i + DLB2_LSP_QID2CQIDIX2_00_CQ_P1_LOC));
		break;

	case 2:
		atm_qid2cq &= ~(1 << (i + DLB2_ATM_QID2CQIDIX_00_CQ_P2_LOC));
		lsp_qid2cq &= ~(1 << (i + DLB2_LSP_QID2CQIDIX_00_CQ_P2_LOC));
		lsp_qid2cq2 &= ~(1 << (i + DLB2_LSP_QID2CQIDIX2_00_CQ_P2_LOC));
		break;

	case 3:
		atm_qid2cq &= ~(1 << (i + DLB2_ATM_QID2CQIDIX_00_CQ_P3_LOC));
		lsp_qid2cq &= ~(1 << (i + DLB2_LSP_QID2CQIDIX_00_CQ_P3_LOC));
		lsp_qid2cq2 &= ~(1 << (i + DLB2_LSP_QID2CQIDIX2_00_CQ_P3_LOC));
		break;
	}

	DLB2_CSR_WR(hw, DLB2_ATM_QID2CQIDIX(queue_id, port_id / 4), atm_qid2cq);

	DLB2_CSR_WR(hw, DLB2_LSP_QID2CQIDIX(hw->ver, queue_id, port_id / 4),
		    lsp_qid2cq);

	DLB2_CSR_WR(hw, DLB2_LSP_QID2CQIDIX2(hw->ver, queue_id, port_id / 4),
		    lsp_qid2cq2);

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

	/* Re-enable the CQ if it was not manually disabled by the user */
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
	u32 infl_cnt;
	int i;
	const int max_iters = 1000;
	const int iter_poll_us = 100;

	if (port->num_pending_removals == 0)
		return false;

	/*
	 * The unmap requires all the CQ's outstanding inflights to be
	 * completed. Poll up to 100ms.
	 */
	for (i = 0; i < max_iters; i++) {
		infl_cnt = DLB2_CSR_RD(hw, DLB2_LSP_CQ_LDB_INFL_CNT(hw->ver,
						       port->id.phys_id));

		if (DLB2_BITS_GET(infl_cnt,
				  DLB2_LSP_CQ_LDB_INFL_CNT_COUNT) == 0)
			break;
		rte_delay_us_sleep(iter_poll_us);
	}

	if (DLB2_BITS_GET(infl_cnt, DLB2_LSP_CQ_LDB_INFL_CNT_COUNT) > 0)
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
	struct dlb2_dir_pq_pair *port;
	u32 vpp_v = 0;
	RTE_SET_USED(iter);

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter) {
		unsigned int offs;
		u32 virt_id;

		if (hw->virt_mode == DLB2_VIRT_SRIOV)
			virt_id = port->id.virt_id;
		else
			virt_id = port->id.phys_id;

		offs = vdev_id * DLB2_MAX_NUM_DIR_PORTS(hw->ver) + virt_id;

		DLB2_CSR_WR(hw, DLB2_SYS_VF_DIR_VPP_V(offs), vpp_v);
	}
}

static void dlb2_domain_disable_ldb_vpps(struct dlb2_hw *hw,
					 struct dlb2_hw_domain *domain,
					 unsigned int vdev_id)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_port *port;
	u32 vpp_v = 0;
	int i;
	RTE_SET_USED(iter);

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			unsigned int offs;
			u32 virt_id;

			if (hw->virt_mode == DLB2_VIRT_SRIOV)
				virt_id = port->id.virt_id;
			else
				virt_id = port->id.phys_id;

			offs = vdev_id * DLB2_MAX_NUM_LDB_PORTS + virt_id;

			DLB2_CSR_WR(hw, DLB2_SYS_VF_LDB_VPP_V(offs), vpp_v);
		}
	}
}

static void
dlb2_domain_disable_ldb_port_interrupts(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_port *port;
	u32 int_en = 0;
	u32 wd_en = 0;
	int i;
	RTE_SET_USED(iter);

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			DLB2_CSR_WR(hw,
				    DLB2_CHP_LDB_CQ_INT_ENB(hw->ver,
						       port->id.phys_id),
				    int_en);

			DLB2_CSR_WR(hw,
				    DLB2_CHP_LDB_CQ_WD_ENB(hw->ver,
						      port->id.phys_id),
				    wd_en);
		}
	}
}

static void
dlb2_domain_disable_dir_port_interrupts(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_dir_pq_pair *port;
	u32 int_en = 0;
	u32 wd_en = 0;
	RTE_SET_USED(iter);

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter) {
		DLB2_CSR_WR(hw,
			    DLB2_CHP_DIR_CQ_INT_ENB(hw->ver, port->id.phys_id),
			    int_en);

		DLB2_CSR_WR(hw,
			    DLB2_CHP_DIR_CQ_WD_ENB(hw->ver, port->id.phys_id),
			    wd_en);
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
		int idx = domain_offset + queue->id.phys_id;

		DLB2_CSR_WR(hw, DLB2_SYS_LDB_VASQID_V(idx), 0);

		if (queue->id.vdev_owned) {
			DLB2_CSR_WR(hw,
				    DLB2_SYS_LDB_QID2VQID(queue->id.phys_id),
				    0);

			idx = queue->id.vdev_id * DLB2_MAX_NUM_LDB_QUEUES +
				queue->id.virt_id;

			DLB2_CSR_WR(hw, DLB2_SYS_VF_LDB_VQID_V(idx), 0);

			DLB2_CSR_WR(hw, DLB2_SYS_VF_LDB_VQID2QID(idx), 0);
		}
	}
}

static void
dlb2_domain_disable_dir_queue_write_perms(struct dlb2_hw *hw,
					  struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_dir_pq_pair *queue;
	unsigned long max_ports;
	int domain_offset;
	RTE_SET_USED(iter);

	max_ports = DLB2_MAX_NUM_DIR_PORTS(hw->ver);

	domain_offset = domain->id.phys_id * max_ports;

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, queue, iter) {
		int idx = domain_offset + queue->id.phys_id;

		DLB2_CSR_WR(hw, DLB2_SYS_DIR_VASQID_V(idx), 0);

		if (queue->id.vdev_owned) {
			idx = queue->id.vdev_id * max_ports + queue->id.virt_id;

			DLB2_CSR_WR(hw, DLB2_SYS_VF_DIR_VQID_V(idx), 0);

			DLB2_CSR_WR(hw, DLB2_SYS_VF_DIR_VQID2QID(idx), 0);
		}
	}
}

static void dlb2_domain_disable_ldb_seq_checks(struct dlb2_hw *hw,
					       struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_port *port;
	u32 chk_en = 0;
	int i;
	RTE_SET_USED(iter);

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			DLB2_CSR_WR(hw,
				    DLB2_CHP_SN_CHK_ENBL(hw->ver,
							 port->id.phys_id),
				    chk_en);
		}
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
			int j;

			for (j = 0; j < DLB2_MAX_CQ_COMP_CHECK_LOOPS; j++) {
				if (dlb2_ldb_cq_inflight_count(hw, port) == 0)
					break;
			}

			if (j == DLB2_MAX_CQ_COMP_CHECK_LOOPS) {
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
	u32 pp_v = 0;
	RTE_SET_USED(iter);

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter) {
		DLB2_CSR_WR(hw,
			    DLB2_SYS_DIR_PP_V(port->id.phys_id),
			    pp_v);
	}
}

static void
dlb2_domain_disable_ldb_producer_ports(struct dlb2_hw *hw,
				       struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_port *port;
	u32 pp_v = 0;
	int i;
	RTE_SET_USED(iter);

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			DLB2_CSR_WR(hw,
				    DLB2_SYS_LDB_PP_V(port->id.phys_id),
				    pp_v);
		}
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
		    DLB2_CHP_LDB_CQ2VAS(hw->ver, port->id.phys_id),
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
		    DLB2_LSP_CQ_LDB_DSBL(hw->ver, port->id.phys_id),
		    DLB2_LSP_CQ_LDB_DSBL_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_LDB_CQ_DEPTH(hw->ver, port->id.phys_id),
		    DLB2_CHP_LDB_CQ_DEPTH_RST);

	if (hw->ver != DLB2_HW_V2)
		DLB2_CSR_WR(hw,
			    DLB2_LSP_CFG_CQ_LDB_WU_LIMIT(port->id.phys_id),
			    DLB2_LSP_CFG_CQ_LDB_WU_LIMIT_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_LDB_INFL_LIM(hw->ver, port->id.phys_id),
		    DLB2_LSP_CQ_LDB_INFL_LIM_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_HIST_LIST_LIM(hw->ver, port->id.phys_id),
		    DLB2_CHP_HIST_LIST_LIM_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_HIST_LIST_BASE(hw->ver, port->id.phys_id),
		    DLB2_CHP_HIST_LIST_BASE_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_HIST_LIST_POP_PTR(hw->ver, port->id.phys_id),
		    DLB2_CHP_HIST_LIST_POP_PTR_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_HIST_LIST_PUSH_PTR(hw->ver, port->id.phys_id),
		    DLB2_CHP_HIST_LIST_PUSH_PTR_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_LDB_CQ_INT_DEPTH_THRSH(hw->ver, port->id.phys_id),
		    DLB2_CHP_LDB_CQ_INT_DEPTH_THRSH_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_LDB_CQ_TMR_THRSH(hw->ver, port->id.phys_id),
		    DLB2_CHP_LDB_CQ_TMR_THRSH_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_LDB_CQ_INT_ENB(hw->ver, port->id.phys_id),
		    DLB2_CHP_LDB_CQ_INT_ENB_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_LDB_CQ_ISR(port->id.phys_id),
		    DLB2_SYS_LDB_CQ_ISR_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_LDB_TKN_DEPTH_SEL(hw->ver, port->id.phys_id),
		    DLB2_LSP_CQ_LDB_TKN_DEPTH_SEL_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_LDB_CQ_TKN_DEPTH_SEL(hw->ver, port->id.phys_id),
		    DLB2_CHP_LDB_CQ_TKN_DEPTH_SEL_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_LDB_CQ_WPTR(hw->ver, port->id.phys_id),
		    DLB2_CHP_LDB_CQ_WPTR_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_LDB_TKN_CNT(hw->ver, port->id.phys_id),
		    DLB2_LSP_CQ_LDB_TKN_CNT_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_LDB_CQ_ADDR_L(port->id.phys_id),
		    DLB2_SYS_LDB_CQ_ADDR_L_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_LDB_CQ_ADDR_U(port->id.phys_id),
		    DLB2_SYS_LDB_CQ_ADDR_U_RST);

	if (hw->ver == DLB2_HW_V2)
		DLB2_CSR_WR(hw,
			    DLB2_SYS_LDB_CQ_AT(port->id.phys_id),
			    DLB2_SYS_LDB_CQ_AT_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_LDB_CQ_PASID(hw->ver, port->id.phys_id),
		    DLB2_SYS_LDB_CQ_PASID_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_LDB_CQ2VF_PF_RO(port->id.phys_id),
		    DLB2_SYS_LDB_CQ2VF_PF_RO_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_LDB_TOT_SCH_CNTL(hw->ver, port->id.phys_id),
		    DLB2_LSP_CQ_LDB_TOT_SCH_CNTL_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_LDB_TOT_SCH_CNTH(hw->ver, port->id.phys_id),
		    DLB2_LSP_CQ_LDB_TOT_SCH_CNTH_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ2QID0(hw->ver, port->id.phys_id),
		    DLB2_LSP_CQ2QID0_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ2QID1(hw->ver, port->id.phys_id),
		    DLB2_LSP_CQ2QID1_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ2PRIOV(hw->ver, port->id.phys_id),
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
	u32 reg = 0;

	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ2VAS(hw->ver, port->id.phys_id),
		    DLB2_CHP_DIR_CQ2VAS_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_DIR_DSBL(hw->ver, port->id.phys_id),
		    DLB2_LSP_CQ_DIR_DSBL_RST);

	DLB2_BIT_SET(reg, DLB2_SYS_WB_DIR_CQ_STATE_CQ_OPT_CLR);

	if (hw->ver == DLB2_HW_V2)
		DLB2_CSR_WR(hw, DLB2_SYS_DIR_CQ_OPT_CLR, port->id.phys_id);
	else
		DLB2_CSR_WR(hw,
			    DLB2_SYS_WB_DIR_CQ_STATE(port->id.phys_id), reg);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ_DEPTH(hw->ver, port->id.phys_id),
		    DLB2_CHP_DIR_CQ_DEPTH_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ_INT_DEPTH_THRSH(hw->ver, port->id.phys_id),
		    DLB2_CHP_DIR_CQ_INT_DEPTH_THRSH_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ_TMR_THRSH(hw->ver, port->id.phys_id),
		    DLB2_CHP_DIR_CQ_TMR_THRSH_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ_INT_ENB(hw->ver, port->id.phys_id),
		    DLB2_CHP_DIR_CQ_INT_ENB_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_CQ_ISR(port->id.phys_id),
		    DLB2_SYS_DIR_CQ_ISR_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_DIR_TKN_DEPTH_SEL_DSI(hw->ver,
						      port->id.phys_id),
		    DLB2_LSP_CQ_DIR_TKN_DEPTH_SEL_DSI_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ_TKN_DEPTH_SEL(hw->ver, port->id.phys_id),
		    DLB2_CHP_DIR_CQ_TKN_DEPTH_SEL_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ_WPTR(hw->ver, port->id.phys_id),
		    DLB2_CHP_DIR_CQ_WPTR_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_DIR_TKN_CNT(hw->ver, port->id.phys_id),
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

	if (hw->ver == DLB2_HW_V2)
		DLB2_CSR_WR(hw,
			    DLB2_SYS_DIR_CQ_AT(port->id.phys_id),
			    DLB2_SYS_DIR_CQ_AT_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_CQ_PASID(hw->ver, port->id.phys_id),
		    DLB2_SYS_DIR_CQ_PASID_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_CQ_FMT(port->id.phys_id),
		    DLB2_SYS_DIR_CQ_FMT_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_CQ2VF_PF_RO(port->id.phys_id),
		    DLB2_SYS_DIR_CQ2VF_PF_RO_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_DIR_TOT_SCH_CNTL(hw->ver, port->id.phys_id),
		    DLB2_LSP_CQ_DIR_TOT_SCH_CNTL_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_DIR_TOT_SCH_CNTH(hw->ver, port->id.phys_id),
		    DLB2_LSP_CQ_DIR_TOT_SCH_CNTH_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_PP2VAS(port->id.phys_id),
		    DLB2_SYS_DIR_PP2VAS_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ2VAS(hw->ver, port->id.phys_id),
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

		offs = port->id.vdev_id * DLB2_MAX_NUM_DIR_PORTS(hw->ver) +
			virt_id;

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
			    DLB2_LSP_QID_NALDB_TOT_ENQ_CNTL(hw->ver, queue_id),
			    DLB2_LSP_QID_NALDB_TOT_ENQ_CNTL_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_NALDB_TOT_ENQ_CNTH(hw->ver, queue_id),
			    DLB2_LSP_QID_NALDB_TOT_ENQ_CNTH_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_ATM_TOT_ENQ_CNTL(hw->ver, queue_id),
			    DLB2_LSP_QID_ATM_TOT_ENQ_CNTL_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_ATM_TOT_ENQ_CNTH(hw->ver, queue_id),
			    DLB2_LSP_QID_ATM_TOT_ENQ_CNTH_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_NALDB_MAX_DEPTH(hw->ver, queue_id),
			    DLB2_LSP_QID_NALDB_MAX_DEPTH_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_LDB_INFL_LIM(hw->ver, queue_id),
			    DLB2_LSP_QID_LDB_INFL_LIM_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_AQED_ACTIVE_LIM(hw->ver, queue_id),
			    DLB2_LSP_QID_AQED_ACTIVE_LIM_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_ATM_DEPTH_THRSH(hw->ver, queue_id),
			    DLB2_LSP_QID_ATM_DEPTH_THRSH_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_NALDB_DEPTH_THRSH(hw->ver, queue_id),
			    DLB2_LSP_QID_NALDB_DEPTH_THRSH_RST);

		DLB2_CSR_WR(hw,
			    DLB2_SYS_LDB_QID_ITS(queue_id),
			    DLB2_SYS_LDB_QID_ITS_RST);

		DLB2_CSR_WR(hw,
			    DLB2_CHP_ORD_QID_SN(hw->ver, queue_id),
			    DLB2_CHP_ORD_QID_SN_RST);

		DLB2_CSR_WR(hw,
			    DLB2_CHP_ORD_QID_SN_MAP(hw->ver, queue_id),
			    DLB2_CHP_ORD_QID_SN_MAP_RST);

		DLB2_CSR_WR(hw,
			    DLB2_SYS_LDB_QID_V(queue_id),
			    DLB2_SYS_LDB_QID_V_RST);

		DLB2_CSR_WR(hw,
			    DLB2_SYS_LDB_QID_CFG_V(queue_id),
			    DLB2_SYS_LDB_QID_CFG_V_RST);

		if (queue->sn_cfg_valid) {
			u32 offs[2];

			offs[0] = DLB2_RO_GRP_0_SLT_SHFT(hw->ver,
							 queue->sn_slot);
			offs[1] = DLB2_RO_GRP_1_SLT_SHFT(hw->ver,
							 queue->sn_slot);

			DLB2_CSR_WR(hw,
				    offs[queue->sn_group],
				    DLB2_RO_GRP_0_SLT_SHFT_RST);
		}

		for (i = 0; i < DLB2_LSP_QID2CQIDIX_NUM; i++) {
			DLB2_CSR_WR(hw,
				    DLB2_LSP_QID2CQIDIX(hw->ver, queue_id, i),
				    DLB2_LSP_QID2CQIDIX_00_RST);

			DLB2_CSR_WR(hw,
				    DLB2_LSP_QID2CQIDIX2(hw->ver, queue_id, i),
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
			    DLB2_LSP_QID_DIR_MAX_DEPTH(hw->ver,
						       queue->id.phys_id),
			    DLB2_LSP_QID_DIR_MAX_DEPTH_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_DIR_TOT_ENQ_CNTL(hw->ver,
							  queue->id.phys_id),
			    DLB2_LSP_QID_DIR_TOT_ENQ_CNTL_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_DIR_TOT_ENQ_CNTH(hw->ver,
							  queue->id.phys_id),
			    DLB2_LSP_QID_DIR_TOT_ENQ_CNTH_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_DIR_DEPTH_THRSH(hw->ver,
							 queue->id.phys_id),
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

	if (hw->ver == DLB2_HW_V2) {
		DLB2_CSR_WR(hw,
			    DLB2_CHP_CFG_LDB_VAS_CRD(domain->id.phys_id),
			    DLB2_CHP_CFG_LDB_VAS_CRD_RST);

		DLB2_CSR_WR(hw,
			    DLB2_CHP_CFG_DIR_VAS_CRD(domain->id.phys_id),
			    DLB2_CHP_CFG_DIR_VAS_CRD_RST);
	} else
		DLB2_CSR_WR(hw,
			    DLB2_CHP_CFG_VAS_CRD(domain->id.phys_id),
			    DLB2_CHP_CFG_VAS_CRD_RST);
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
			ldb_port->cq_depth = 0;
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
			    "[%s()] Internal error: domain hist list base does not match the function's bitmap.\n",
			    __func__);
		return ret;
	}

	domain->total_hist_list_entries = 0;
	domain->avail_hist_list_entries = 0;
	domain->hist_list_entry_base = 0;
	domain->hist_list_entry_offset = 0;

	if (hw->ver == DLB2_HW_V2_5) {
		rsrcs->num_avail_entries += domain->num_credits;
		domain->num_credits = 0;
	} else {
		rsrcs->num_avail_qed_entries += domain->num_ldb_credits;
		domain->num_ldb_credits = 0;

		rsrcs->num_avail_dqed_entries += domain->num_dir_credits;
		domain->num_dir_credits = 0;
	}
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
	struct dlb2_ldb_port *port = NULL;
	int ret, i;

	/* If a domain has LDB queues, it must have LDB ports */
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		port = DLB2_DOM_LIST_HEAD(domain->used_ldb_ports[i],
					  typeof(*port));
		if (port)
			break;
	}

	if (port == NULL) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: No configured LDB ports\n",
			    __func__);
		return -EFAULT;
	}

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
 * dlb2_reset_domain() - reset a scheduling domain
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function resets and frees a DLB 2.0 scheduling domain and its associated
 * resources.
 *
 * Pre-condition: the driver must ensure software has stopped sending QEs
 * through this domain's producer ports before invoking this function, or
 * undefined behavior will result.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, -1 otherwise.
 *
 * EINVAL - Invalid domain ID, or the domain is not configured.
 * EFAULT - Internal error. (Possibly caused if software is the pre-condition
 *	    is not met.)
 * ETIMEDOUT - Hardware component didn't reset in the expected time.
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

	if (domain == NULL || !domain->configured)
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

	dlb2_domain_drain_ldb_cqs(hw, domain, false);

	ret = dlb2_domain_wait_for_ldb_cqs_to_empty(hw, domain);
	if (ret)
		return ret;

	ret = dlb2_domain_finish_unmap_qid_procedures(hw, domain);
	if (ret)
		return ret;

	ret = dlb2_domain_finish_map_qid_procedures(hw, domain);
	if (ret)
		return ret;

	/* Re-enable the CQs in order to drain the mapped queues. */
	dlb2_domain_enable_ldb_cqs(hw, domain);

	ret = dlb2_domain_drain_mapped_queues(hw, domain);
	if (ret)
		return ret;

	ret = dlb2_domain_drain_unmapped_queues(hw, domain);
	if (ret)
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
	return dlb2_domain_reset_software_state(hw, domain);
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
dlb2_verify_create_ldb_queue_args(struct dlb2_hw *hw,
				  u32 domain_id,
				  struct dlb2_create_ldb_queue_args *args,
				  struct dlb2_cmd_response *resp,
				  bool vdev_req,
				  unsigned int vdev_id,
				  struct dlb2_hw_domain **out_domain,
				  struct dlb2_ldb_queue **out_queue)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_queue *queue;
	int i;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
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

	queue = DLB2_DOM_LIST_HEAD(domain->avail_ldb_queues, typeof(*queue));
	if (!queue) {
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

	if (args->num_qid_inflights < 1 || args->num_qid_inflights > 2048) {
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

	*out_domain = domain;
	*out_queue = queue;

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

static void dlb2_configure_ldb_queue(struct dlb2_hw *hw,
				     struct dlb2_hw_domain *domain,
				     struct dlb2_ldb_queue *queue,
				     struct dlb2_create_ldb_queue_args *args,
				     bool vdev_req,
				     unsigned int vdev_id)
{
	struct dlb2_sn_group *sn_group;
	unsigned int offs;
	u32 reg = 0;
	u32 alimit;

	/* QID write permissions are turned on when the domain is started */
	offs = domain->id.phys_id * DLB2_MAX_NUM_LDB_QUEUES + queue->id.phys_id;

	DLB2_CSR_WR(hw, DLB2_SYS_LDB_VASQID_V(offs), reg);

	/*
	 * Unordered QIDs get 4K inflights, ordered get as many as the number
	 * of sequence numbers.
	 */
	DLB2_BITS_SET(reg, args->num_qid_inflights,
		      DLB2_LSP_QID_LDB_INFL_LIM_LIMIT);
	DLB2_CSR_WR(hw, DLB2_LSP_QID_LDB_INFL_LIM(hw->ver,
						  queue->id.phys_id), reg);

	alimit = queue->aqed_limit;

	if (alimit > DLB2_MAX_NUM_AQED_ENTRIES)
		alimit = DLB2_MAX_NUM_AQED_ENTRIES;

	reg = 0;
	DLB2_BITS_SET(reg, alimit, DLB2_LSP_QID_AQED_ACTIVE_LIM_LIMIT);
	DLB2_CSR_WR(hw,
		    DLB2_LSP_QID_AQED_ACTIVE_LIM(hw->ver,
						 queue->id.phys_id), reg);

	reg = 0;
	switch (args->lock_id_comp_level) {
	case 64:
		DLB2_BITS_SET(reg, 1, DLB2_AQED_QID_HID_WIDTH_COMPRESS_CODE);
		break;
	case 128:
		DLB2_BITS_SET(reg, 2, DLB2_AQED_QID_HID_WIDTH_COMPRESS_CODE);
		break;
	case 256:
		DLB2_BITS_SET(reg, 3, DLB2_AQED_QID_HID_WIDTH_COMPRESS_CODE);
		break;
	case 512:
		DLB2_BITS_SET(reg, 4, DLB2_AQED_QID_HID_WIDTH_COMPRESS_CODE);
		break;
	case 1024:
		DLB2_BITS_SET(reg, 5, DLB2_AQED_QID_HID_WIDTH_COMPRESS_CODE);
		break;
	case 2048:
		DLB2_BITS_SET(reg, 6, DLB2_AQED_QID_HID_WIDTH_COMPRESS_CODE);
		break;
	case 4096:
		DLB2_BITS_SET(reg, 7, DLB2_AQED_QID_HID_WIDTH_COMPRESS_CODE);
		break;
	default:
		/* No compression by default */
		break;
	}

	DLB2_CSR_WR(hw, DLB2_AQED_QID_HID_WIDTH(queue->id.phys_id), reg);

	reg = 0;
	/* Don't timestamp QEs that pass through this queue */
	DLB2_CSR_WR(hw, DLB2_SYS_LDB_QID_ITS(queue->id.phys_id), reg);

	DLB2_BITS_SET(reg, args->depth_threshold,
		      DLB2_LSP_QID_ATM_DEPTH_THRSH_THRESH);
	DLB2_CSR_WR(hw,
		    DLB2_LSP_QID_ATM_DEPTH_THRSH(hw->ver,
						 queue->id.phys_id), reg);

	reg = 0;
	DLB2_BITS_SET(reg, args->depth_threshold,
		      DLB2_LSP_QID_NALDB_DEPTH_THRSH_THRESH);
	DLB2_CSR_WR(hw,
		    DLB2_LSP_QID_NALDB_DEPTH_THRSH(hw->ver, queue->id.phys_id),
		    reg);

	/*
	 * This register limits the number of inflight flows a queue can have
	 * at one time.  It has an upper bound of 2048, but can be
	 * over-subscribed. 512 is chosen so that a single queue does not use
	 * the entire atomic storage, but can use a substantial portion if
	 * needed.
	 */
	reg = 0;
	DLB2_BITS_SET(reg, 512, DLB2_AQED_QID_FID_LIM_QID_FID_LIMIT);
	DLB2_CSR_WR(hw, DLB2_AQED_QID_FID_LIM(queue->id.phys_id), reg);

	/* Configure SNs */
	reg = 0;
	sn_group = &hw->rsrcs.sn_groups[queue->sn_group];
	DLB2_BITS_SET(reg, sn_group->mode, DLB2_CHP_ORD_QID_SN_MAP_MODE);
	DLB2_BITS_SET(reg, queue->sn_slot, DLB2_CHP_ORD_QID_SN_MAP_SLOT);
	DLB2_BITS_SET(reg, sn_group->id, DLB2_CHP_ORD_QID_SN_MAP_GRP);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_ORD_QID_SN_MAP(hw->ver, queue->id.phys_id), reg);

	reg = 0;
	DLB2_BITS_SET(reg, (args->num_sequence_numbers != 0),
		 DLB2_SYS_LDB_QID_CFG_V_SN_CFG_V);
	DLB2_BITS_SET(reg, (args->num_atomic_inflights != 0),
		 DLB2_SYS_LDB_QID_CFG_V_FID_CFG_V);

	DLB2_CSR_WR(hw, DLB2_SYS_LDB_QID_CFG_V(queue->id.phys_id), reg);

	if (vdev_req) {
		offs = vdev_id * DLB2_MAX_NUM_LDB_QUEUES + queue->id.virt_id;

		reg = 0;
		DLB2_BIT_SET(reg, DLB2_SYS_VF_LDB_VQID_V_VQID_V);
		DLB2_CSR_WR(hw, DLB2_SYS_VF_LDB_VQID_V(offs), reg);

		reg = 0;
		DLB2_BITS_SET(reg, queue->id.phys_id,
			      DLB2_SYS_VF_LDB_VQID2QID_QID);
		DLB2_CSR_WR(hw, DLB2_SYS_VF_LDB_VQID2QID(offs), reg);

		reg = 0;
		DLB2_BITS_SET(reg, queue->id.virt_id,
			      DLB2_SYS_LDB_QID2VQID_VQID);
		DLB2_CSR_WR(hw, DLB2_SYS_LDB_QID2VQID(queue->id.phys_id), reg);
	}

	reg = 0;
	DLB2_BIT_SET(reg, DLB2_SYS_LDB_QID_V_QID_V);
	DLB2_CSR_WR(hw, DLB2_SYS_LDB_QID_V(queue->id.phys_id), reg);
}

/**
 * dlb2_hw_create_ldb_queue() - create a load-balanced queue
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: queue creation arguments.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function creates a load-balanced queue.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the queue ID.
 *
 * resp->id contains a virtual ID if vdev_req is true.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, the domain is not configured,
 *	    the domain has already been started, or the requested queue name is
 *	    already in use.
 * EFAULT - Internal error (resp->status not set).
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
						vdev_id,
						&domain,
						&queue);
	if (ret)
		return ret;

	ret = dlb2_ldb_queue_attach_resources(hw, domain, queue, args);

	if (ret) {
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

static void dlb2_ldb_port_configure_pp(struct dlb2_hw *hw,
				       struct dlb2_hw_domain *domain,
				       struct dlb2_ldb_port *port,
				       bool vdev_req,
				       unsigned int vdev_id)
{
	u32 reg = 0;

	DLB2_BITS_SET(reg, domain->id.phys_id, DLB2_SYS_LDB_PP2VAS_VAS);
	DLB2_CSR_WR(hw, DLB2_SYS_LDB_PP2VAS(port->id.phys_id), reg);

	if (vdev_req) {
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

		reg = 0;
		DLB2_BITS_SET(reg, port->id.phys_id, DLB2_SYS_VF_LDB_VPP2PP_PP);
		offs = vdev_id * DLB2_MAX_NUM_LDB_PORTS + virt_id;
		DLB2_CSR_WR(hw, DLB2_SYS_VF_LDB_VPP2PP(offs), reg);

		reg = 0;
		DLB2_BITS_SET(reg, vdev_id, DLB2_SYS_LDB_PP2VDEV_VDEV);
		DLB2_CSR_WR(hw, DLB2_SYS_LDB_PP2VDEV(port->id.phys_id), reg);

		reg = 0;
		DLB2_BIT_SET(reg, DLB2_SYS_VF_LDB_VPP_V_VPP_V);
		DLB2_CSR_WR(hw, DLB2_SYS_VF_LDB_VPP_V(offs), reg);
	}

	reg = 0;
	DLB2_BIT_SET(reg, DLB2_SYS_LDB_PP_V_PP_V);
	DLB2_CSR_WR(hw, DLB2_SYS_LDB_PP_V(port->id.phys_id), reg);
}

static int dlb2_ldb_port_configure_cq(struct dlb2_hw *hw,
				      struct dlb2_hw_domain *domain,
				      struct dlb2_ldb_port *port,
				      uintptr_t cq_dma_base,
				      struct dlb2_create_ldb_port_args *args,
				      bool vdev_req,
				      unsigned int vdev_id)
{
	u32 hl_base = 0;
	u32 reg = 0;
	u32 ds = 0;

	/* The CQ address is 64B-aligned, and the DLB only wants bits [63:6] */
	DLB2_BITS_SET(reg, cq_dma_base >> 6, DLB2_SYS_LDB_CQ_ADDR_L_ADDR_L);
	DLB2_CSR_WR(hw, DLB2_SYS_LDB_CQ_ADDR_L(port->id.phys_id), reg);

	reg = cq_dma_base >> 32;
	DLB2_CSR_WR(hw, DLB2_SYS_LDB_CQ_ADDR_U(port->id.phys_id), reg);

	/*
	 * 'ro' == relaxed ordering. This setting allows DLB2 to write
	 * cache lines out-of-order (but QEs within a cache line are always
	 * updated in-order).
	 */
	reg = 0;
	DLB2_BITS_SET(reg, vdev_id, DLB2_SYS_LDB_CQ2VF_PF_RO_VF);
	DLB2_BITS_SET(reg,
		 !vdev_req && (hw->virt_mode != DLB2_VIRT_SIOV),
		 DLB2_SYS_LDB_CQ2VF_PF_RO_IS_PF);
	DLB2_BIT_SET(reg, DLB2_SYS_LDB_CQ2VF_PF_RO_RO);

	DLB2_CSR_WR(hw, DLB2_SYS_LDB_CQ2VF_PF_RO(port->id.phys_id), reg);

	port->cq_depth = args->cq_depth;

	if (args->cq_depth <= 8) {
		ds = 1;
	} else if (args->cq_depth == 16) {
		ds = 2;
	} else if (args->cq_depth == 32) {
		ds = 3;
	} else if (args->cq_depth == 64) {
		ds = 4;
	} else if (args->cq_depth == 128) {
		ds = 5;
	} else if (args->cq_depth == 256) {
		ds = 6;
	} else if (args->cq_depth == 512) {
		ds = 7;
	} else if (args->cq_depth == 1024) {
		ds = 8;
	} else {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: invalid CQ depth\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	reg = 0;
	DLB2_BITS_SET(reg, ds,
		      DLB2_CHP_LDB_CQ_TKN_DEPTH_SEL_TOKEN_DEPTH_SELECT);
	DLB2_CSR_WR(hw,
		    DLB2_CHP_LDB_CQ_TKN_DEPTH_SEL(hw->ver, port->id.phys_id),
		    reg);

	/*
	 * To support CQs with depth less than 8, program the token count
	 * register with a non-zero initial value. Operations such as domain
	 * reset must take this initial value into account when quiescing the
	 * CQ.
	 */
	port->init_tkn_cnt = 0;

	if (args->cq_depth < 8) {
		reg = 0;
		port->init_tkn_cnt = 8 - args->cq_depth;

		DLB2_BITS_SET(reg,
			      port->init_tkn_cnt,
			      DLB2_LSP_CQ_LDB_TKN_CNT_TOKEN_COUNT);
		DLB2_CSR_WR(hw,
			    DLB2_LSP_CQ_LDB_TKN_CNT(hw->ver, port->id.phys_id),
			    reg);
	} else {
		DLB2_CSR_WR(hw,
			    DLB2_LSP_CQ_LDB_TKN_CNT(hw->ver, port->id.phys_id),
			    DLB2_LSP_CQ_LDB_TKN_CNT_RST);
	}

	reg = 0;
	DLB2_BITS_SET(reg, ds,
		      DLB2_LSP_CQ_LDB_TKN_DEPTH_SEL_TOKEN_DEPTH_SELECT_V2);
	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_LDB_TKN_DEPTH_SEL(hw->ver, port->id.phys_id),
		    reg);

	/* Reset the CQ write pointer */
	DLB2_CSR_WR(hw,
		    DLB2_CHP_LDB_CQ_WPTR(hw->ver, port->id.phys_id),
		    DLB2_CHP_LDB_CQ_WPTR_RST);

	reg = 0;
	DLB2_BITS_SET(reg,
		      port->hist_list_entry_limit - 1,
		      DLB2_CHP_HIST_LIST_LIM_LIMIT);
	DLB2_CSR_WR(hw, DLB2_CHP_HIST_LIST_LIM(hw->ver, port->id.phys_id), reg);

	DLB2_BITS_SET(hl_base, port->hist_list_entry_base,
		      DLB2_CHP_HIST_LIST_BASE_BASE);
	DLB2_CSR_WR(hw,
		    DLB2_CHP_HIST_LIST_BASE(hw->ver, port->id.phys_id),
		    hl_base);

	/*
	 * The inflight limit sets a cap on the number of QEs for which this CQ
	 * can owe completions at one time.
	 */
	reg = 0;
	DLB2_BITS_SET(reg, args->cq_history_list_size,
		      DLB2_LSP_CQ_LDB_INFL_LIM_LIMIT);
	DLB2_CSR_WR(hw, DLB2_LSP_CQ_LDB_INFL_LIM(hw->ver, port->id.phys_id),
		    reg);

	reg = 0;
	DLB2_BITS_SET(reg, DLB2_BITS_GET(hl_base, DLB2_CHP_HIST_LIST_BASE_BASE),
		      DLB2_CHP_HIST_LIST_PUSH_PTR_PUSH_PTR);
	DLB2_CSR_WR(hw, DLB2_CHP_HIST_LIST_PUSH_PTR(hw->ver, port->id.phys_id),
		    reg);

	reg = 0;
	DLB2_BITS_SET(reg, DLB2_BITS_GET(hl_base, DLB2_CHP_HIST_LIST_BASE_BASE),
		      DLB2_CHP_HIST_LIST_POP_PTR_POP_PTR);
	DLB2_CSR_WR(hw, DLB2_CHP_HIST_LIST_POP_PTR(hw->ver, port->id.phys_id),
		    reg);

	/*
	 * Address translation (AT) settings: 0: untranslated, 2: translated
	 * (see ATS spec regarding Address Type field for more details)
	 */

	if (hw->ver == DLB2_HW_V2) {
		reg = 0;
		DLB2_CSR_WR(hw, DLB2_SYS_LDB_CQ_AT(port->id.phys_id), reg);
	}

	if (vdev_req && hw->virt_mode == DLB2_VIRT_SIOV) {
		reg = 0;
		DLB2_BITS_SET(reg, hw->pasid[vdev_id],
			      DLB2_SYS_LDB_CQ_PASID_PASID);
		DLB2_BIT_SET(reg, DLB2_SYS_LDB_CQ_PASID_FMT2);
	}

	DLB2_CSR_WR(hw, DLB2_SYS_LDB_CQ_PASID(hw->ver, port->id.phys_id), reg);

	reg = 0;
	DLB2_BITS_SET(reg, domain->id.phys_id, DLB2_CHP_LDB_CQ2VAS_CQ2VAS);
	DLB2_CSR_WR(hw, DLB2_CHP_LDB_CQ2VAS(hw->ver, port->id.phys_id), reg);

	/* Disable the port's QID mappings */
	reg = 0;
	DLB2_CSR_WR(hw, DLB2_LSP_CQ2PRIOV(hw->ver, port->id.phys_id), reg);

	return 0;
}

static bool
dlb2_cq_depth_is_valid(u32 depth)
{
	if (depth != 1 && depth != 2 &&
	    depth != 4 && depth != 8 &&
	    depth != 16 && depth != 32 &&
	    depth != 64 && depth != 128 &&
	    depth != 256 && depth != 512 &&
	    depth != 1024)
		return false;

	return true;
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
	if (ret)
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
				 unsigned int vdev_id,
				 struct dlb2_hw_domain **out_domain,
				 struct dlb2_ldb_port **out_port,
				 int *out_cos_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;
	int i, id;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
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

	if (args->cos_id >= DLB2_NUM_COS_DOMAINS &&
	    (args->cos_id != DLB2_COS_DEFAULT || args->cos_strict)) {
		resp->status = DLB2_ST_INVALID_COS_ID;
		return -EINVAL;
	}

	if (args->cos_strict) {
		id = args->cos_id;
		port = DLB2_DOM_LIST_HEAD(domain->avail_ldb_ports[id],
					  typeof(*port));
	} else {
		for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
			if (args->cos_id == DLB2_COS_DEFAULT) {
				/* Allocate from best performing cos */
				u32 cos_idx = i + DLB2_MAX_NUM_LDB_PORTS;
				id = hw->ldb_pp_allocations[cos_idx];
			} else {
				id = (args->cos_id + i) % DLB2_NUM_COS_DOMAINS;
			}

			port = DLB2_DOM_LIST_HEAD(domain->avail_ldb_ports[id],
						  typeof(*port));
			if (port)
				break;
		}
	}

	if (!port) {
		resp->status = DLB2_ST_LDB_PORTS_UNAVAILABLE;
		return -EINVAL;
	}

	DLB2_LOG_INFO(": LDB: cos=%d port:%d\n", id, port->id.phys_id);

	/* Check cache-line alignment */
	if ((cq_dma_base & 0x3F) != 0) {
		resp->status = DLB2_ST_INVALID_CQ_VIRT_ADDR;
		return -EINVAL;
	}

	if (!dlb2_cq_depth_is_valid(args->cq_depth)) {
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

	*out_domain = domain;
	*out_port = port;
	*out_cos_id = id;

	return 0;
}

/**
 * dlb2_hw_create_ldb_port() - create a load-balanced port
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port creation arguments.
 * @cq_dma_base: base address of the CQ memory. This can be a PA or an IOVA.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function creates a load-balanced port.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the port ID.
 *
 * resp->id contains a virtual ID if vdev_req is true.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, a credit setting is invalid, a
 *	    pointer address is not properly aligned, the domain is not
 *	    configured, or the domain has already been started.
 * EFAULT - Internal error (resp->status not set).
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
	int ret, cos_id;

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
					       vdev_id,
					       &domain,
					       &port,
					       &cos_id);
	if (ret)
		return ret;

	ret = dlb2_configure_ldb_port(hw,
				      domain,
				      port,
				      cq_dma_base,
				      args,
				      vdev_req,
				      vdev_id);
	if (ret)
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

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter) {
		if ((!vdev_req && port->id.phys_id == id) ||
		    (vdev_req && port->id.virt_id == id))
			return port;
	}

	return NULL;
}

static int
dlb2_verify_create_dir_port_args(struct dlb2_hw *hw,
				 u32 domain_id,
				 uintptr_t cq_dma_base,
				 struct dlb2_create_dir_port_args *args,
				 struct dlb2_cmd_response *resp,
				 bool vdev_req,
				 unsigned int vdev_id,
				 struct dlb2_hw_domain **out_domain,
				 struct dlb2_dir_pq_pair **out_port)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_dir_pq_pair *pq;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
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

	if (args->queue_id != -1) {
		/*
		 * If the user claims the queue is already configured, validate
		 * the queue ID, its domain, and whether the queue is
		 * configured.
		 */
		pq = dlb2_get_domain_used_dir_pq(hw,
						 args->queue_id,
						 vdev_req,
						 domain);

		if (!pq || pq->domain_id.phys_id != domain->id.phys_id ||
		    !pq->queue_configured) {
			resp->status = DLB2_ST_INVALID_DIR_QUEUE_ID;
			return -EINVAL;
		}
	} else {
		/*
		 * If the port's queue is not configured, validate that a free
		 * port-queue pair is available.
		 * First try the 'res' list if the port is producer OR if
		 * 'avail' list is empty else fall back to 'avail' list
		 */
		if (!dlb2_list_empty(&domain->rsvd_dir_pq_pairs) &&
		    (args->is_producer ||
		     dlb2_list_empty(&domain->avail_dir_pq_pairs)))
			pq = DLB2_DOM_LIST_HEAD(domain->rsvd_dir_pq_pairs,
						typeof(*pq));
		else
			pq = DLB2_DOM_LIST_HEAD(domain->avail_dir_pq_pairs,
						typeof(*pq));

		if (!pq) {
			resp->status = DLB2_ST_DIR_PORTS_UNAVAILABLE;
			return -EINVAL;
		}
		DLB2_LOG_INFO(": DIR: port:%d is_producer=%d\n",
			      pq->id.phys_id, args->is_producer);

	}

	/* Check cache-line alignment */
	if ((cq_dma_base & 0x3F) != 0) {
		resp->status = DLB2_ST_INVALID_CQ_VIRT_ADDR;
		return -EINVAL;
	}

	if (!dlb2_cq_depth_is_valid(args->cq_depth)) {
		resp->status = DLB2_ST_INVALID_CQ_DEPTH;
		return -EINVAL;
	}

	*out_domain = domain;
	*out_port = pq;

	return 0;
}

static void dlb2_dir_port_configure_pp(struct dlb2_hw *hw,
				       struct dlb2_hw_domain *domain,
				       struct dlb2_dir_pq_pair *port,
				       bool vdev_req,
				       unsigned int vdev_id)
{
	u32 reg = 0;

	DLB2_BITS_SET(reg, domain->id.phys_id, DLB2_SYS_DIR_PP2VAS_VAS);
	DLB2_CSR_WR(hw, DLB2_SYS_DIR_PP2VAS(port->id.phys_id), reg);

	if (vdev_req) {
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

		reg = 0;
		DLB2_BITS_SET(reg, port->id.phys_id, DLB2_SYS_VF_DIR_VPP2PP_PP);
		offs = vdev_id * DLB2_MAX_NUM_DIR_PORTS(hw->ver) + virt_id;
		DLB2_CSR_WR(hw, DLB2_SYS_VF_DIR_VPP2PP(offs), reg);

		reg = 0;
		DLB2_BITS_SET(reg, vdev_id, DLB2_SYS_DIR_PP2VDEV_VDEV);
		DLB2_CSR_WR(hw, DLB2_SYS_DIR_PP2VDEV(port->id.phys_id), reg);

		reg = 0;
		DLB2_BIT_SET(reg, DLB2_SYS_VF_DIR_VPP_V_VPP_V);
		DLB2_CSR_WR(hw, DLB2_SYS_VF_DIR_VPP_V(offs), reg);
	}

	reg = 0;
	DLB2_BIT_SET(reg, DLB2_SYS_DIR_PP_V_PP_V);
	DLB2_CSR_WR(hw, DLB2_SYS_DIR_PP_V(port->id.phys_id), reg);
}

static int dlb2_dir_port_configure_cq(struct dlb2_hw *hw,
				      struct dlb2_hw_domain *domain,
				      struct dlb2_dir_pq_pair *port,
				      uintptr_t cq_dma_base,
				      struct dlb2_create_dir_port_args *args,
				      bool vdev_req,
				      unsigned int vdev_id)
{
	u32 reg = 0;
	u32 ds = 0;

	/* The CQ address is 64B-aligned, and the DLB only wants bits [63:6] */
	DLB2_BITS_SET(reg, cq_dma_base >> 6, DLB2_SYS_DIR_CQ_ADDR_L_ADDR_L);
	DLB2_CSR_WR(hw, DLB2_SYS_DIR_CQ_ADDR_L(port->id.phys_id), reg);

	reg = cq_dma_base >> 32;
	DLB2_CSR_WR(hw, DLB2_SYS_DIR_CQ_ADDR_U(port->id.phys_id), reg);

	/*
	 * 'ro' == relaxed ordering. This setting allows DLB2 to write
	 * cache lines out-of-order (but QEs within a cache line are always
	 * updated in-order).
	 */
	reg = 0;
	DLB2_BITS_SET(reg, vdev_id, DLB2_SYS_DIR_CQ2VF_PF_RO_VF);
	DLB2_BITS_SET(reg, !vdev_req && (hw->virt_mode != DLB2_VIRT_SIOV),
		 DLB2_SYS_DIR_CQ2VF_PF_RO_IS_PF);
	DLB2_BIT_SET(reg, DLB2_SYS_DIR_CQ2VF_PF_RO_RO);

	DLB2_CSR_WR(hw, DLB2_SYS_DIR_CQ2VF_PF_RO(port->id.phys_id), reg);

	if (args->cq_depth <= 8) {
		ds = 1;
	} else if (args->cq_depth == 16) {
		ds = 2;
	} else if (args->cq_depth == 32) {
		ds = 3;
	} else if (args->cq_depth == 64) {
		ds = 4;
	} else if (args->cq_depth == 128) {
		ds = 5;
	} else if (args->cq_depth == 256) {
		ds = 6;
	} else if (args->cq_depth == 512) {
		ds = 7;
	} else if (args->cq_depth == 1024) {
		ds = 8;
	} else {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: invalid CQ depth\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	reg = 0;
	DLB2_BITS_SET(reg, ds,
		      DLB2_CHP_DIR_CQ_TKN_DEPTH_SEL_TOKEN_DEPTH_SELECT);
	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ_TKN_DEPTH_SEL(hw->ver, port->id.phys_id),
		    reg);

	/*
	 * To support CQs with depth less than 8, program the token count
	 * register with a non-zero initial value. Operations such as domain
	 * reset must take this initial value into account when quiescing the
	 * CQ.
	 */
	port->init_tkn_cnt = 0;

	if (args->cq_depth < 8) {
		reg = 0;
		port->init_tkn_cnt = 8 - args->cq_depth;

		DLB2_BITS_SET(reg, port->init_tkn_cnt,
			      DLB2_LSP_CQ_DIR_TKN_CNT_COUNT);
		DLB2_CSR_WR(hw,
			    DLB2_LSP_CQ_DIR_TKN_CNT(hw->ver, port->id.phys_id),
			    reg);
	} else {
		DLB2_CSR_WR(hw,
			    DLB2_LSP_CQ_DIR_TKN_CNT(hw->ver, port->id.phys_id),
			    DLB2_LSP_CQ_DIR_TKN_CNT_RST);
	}

	reg = 0;
	DLB2_BITS_SET(reg, ds,
		      DLB2_LSP_CQ_DIR_TKN_DEPTH_SEL_DSI_TOKEN_DEPTH_SELECT_V2);
	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_DIR_TKN_DEPTH_SEL_DSI(hw->ver,
						      port->id.phys_id),
		    reg);

	/* Reset the CQ write pointer */
	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ_WPTR(hw->ver, port->id.phys_id),
		    DLB2_CHP_DIR_CQ_WPTR_RST);

	/* Virtualize the PPID */
	reg = 0;
	DLB2_CSR_WR(hw, DLB2_SYS_DIR_CQ_FMT(port->id.phys_id), reg);

	/*
	 * Address translation (AT) settings: 0: untranslated, 2: translated
	 * (see ATS spec regarding Address Type field for more details)
	 */
	if (hw->ver == DLB2_HW_V2) {
		reg = 0;
		DLB2_CSR_WR(hw, DLB2_SYS_DIR_CQ_AT(port->id.phys_id), reg);
	}

	if (vdev_req && hw->virt_mode == DLB2_VIRT_SIOV) {
		DLB2_BITS_SET(reg, hw->pasid[vdev_id],
			      DLB2_SYS_DIR_CQ_PASID_PASID);
		DLB2_BIT_SET(reg, DLB2_SYS_DIR_CQ_PASID_FMT2);
	}

	DLB2_CSR_WR(hw, DLB2_SYS_DIR_CQ_PASID(hw->ver, port->id.phys_id), reg);

	reg = 0;
	DLB2_BITS_SET(reg, domain->id.phys_id, DLB2_CHP_DIR_CQ2VAS_CQ2VAS);
	DLB2_CSR_WR(hw, DLB2_CHP_DIR_CQ2VAS(hw->ver, port->id.phys_id), reg);

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

	if (ret)
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
 * dlb2_hw_create_dir_port() - create a directed port
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port creation arguments.
 * @cq_dma_base: base address of the CQ memory. This can be a PA or an IOVA.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function creates a directed port.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the port ID.
 *
 * resp->id contains a virtual ID if vdev_req is true.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, a credit setting is invalid, a
 *	    pointer address is not properly aligned, the domain is not
 *	    configured, or the domain has already been started.
 * EFAULT - Internal error (resp->status not set).
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
					       vdev_id,
					       &domain,
					       &port);
	if (ret)
		return ret;

	ret = dlb2_configure_dir_port(hw,
				      domain,
				      port,
				      cq_dma_base,
				      args,
				      vdev_req,
				      vdev_id);
	if (ret)
		return ret;

	/*
	 * Configuration succeeded, so move the resource from the 'avail' or
	 * 'res' to the 'used' list (if it's not already there).
	 */
	if (args->queue_id == -1) {
		struct dlb2_list_head *res = &domain->rsvd_dir_pq_pairs;
		struct dlb2_list_head *avail = &domain->avail_dir_pq_pairs;

		if ((args->is_producer && !dlb2_list_empty(res)) ||
		     dlb2_list_empty(avail))
			dlb2_list_del(res, &port->domain_list);
		else
			dlb2_list_del(avail, &port->domain_list);

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
	unsigned int offs;
	u32 reg = 0;

	/* QID write permissions are turned on when the domain is started */
	offs = domain->id.phys_id * DLB2_MAX_NUM_DIR_QUEUES(hw->ver) +
		queue->id.phys_id;

	DLB2_CSR_WR(hw, DLB2_SYS_DIR_VASQID_V(offs), reg);

	/* Don't timestamp QEs that pass through this queue */
	DLB2_CSR_WR(hw, DLB2_SYS_DIR_QID_ITS(queue->id.phys_id), reg);

	reg = 0;
	DLB2_BITS_SET(reg, args->depth_threshold,
		      DLB2_LSP_QID_DIR_DEPTH_THRSH_THRESH);
	DLB2_CSR_WR(hw,
		    DLB2_LSP_QID_DIR_DEPTH_THRSH(hw->ver, queue->id.phys_id),
		    reg);

	if (vdev_req) {
		offs = vdev_id * DLB2_MAX_NUM_DIR_QUEUES(hw->ver) +
			queue->id.virt_id;

		reg = 0;
		DLB2_BIT_SET(reg, DLB2_SYS_VF_DIR_VQID_V_VQID_V);
		DLB2_CSR_WR(hw, DLB2_SYS_VF_DIR_VQID_V(offs), reg);

		reg = 0;
		DLB2_BITS_SET(reg, queue->id.phys_id,
			      DLB2_SYS_VF_DIR_VQID2QID_QID);
		DLB2_CSR_WR(hw, DLB2_SYS_VF_DIR_VQID2QID(offs), reg);
	}

	reg = 0;
	DLB2_BIT_SET(reg, DLB2_SYS_DIR_QID_V_QID_V);
	DLB2_CSR_WR(hw, DLB2_SYS_DIR_QID_V(queue->id.phys_id), reg);

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
				  unsigned int vdev_id,
				  struct dlb2_hw_domain **out_domain,
				  struct dlb2_dir_pq_pair **out_queue)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_dir_pq_pair *pq;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
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
		pq = dlb2_get_domain_used_dir_pq(hw,
						 args->port_id,
						 vdev_req,
						 domain);

		if (!pq || pq->domain_id.phys_id != domain->id.phys_id ||
		    !pq->port_configured) {
			resp->status = DLB2_ST_INVALID_PORT_ID;
			return -EINVAL;
		}
	} else {
		/*
		 * If the queue's port is not configured, validate that a free
		 * port-queue pair is available.
		 */
		pq = DLB2_DOM_LIST_HEAD(domain->avail_dir_pq_pairs,
					typeof(*pq));
		if (!pq) {
			resp->status = DLB2_ST_DIR_QUEUES_UNAVAILABLE;
			return -EINVAL;
		}
	}

	*out_domain = domain;
	*out_queue = pq;

	return 0;
}

/**
 * dlb2_hw_create_dir_queue() - create a directed queue
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: queue creation arguments.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function creates a directed queue.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the queue ID.
 *
 * resp->id contains a virtual ID if vdev_req is true.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, the domain is not configured,
 *	    or the domain has already been started.
 * EFAULT - Internal error (resp->status not set).
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
						vdev_id,
						&domain,
						&queue);
	if (ret)
		return ret;

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

	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, queue, iter) {
		if ((!vdev_req && queue->id.phys_id == id) ||
		    (vdev_req && queue->id.virt_id == id))
			return queue;
	}

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
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			if ((!vdev_req && port->id.phys_id == id) ||
			    (vdev_req && port->id.virt_id == id))
				return port;
		}

		DLB2_DOM_LIST_FOR(domain->avail_ldb_ports[i], port, iter) {
			if ((!vdev_req && port->id.phys_id == id) ||
			    (vdev_req && port->id.virt_id == id))
				return port;
		}
	}

	return NULL;
}

static void dlb2_ldb_port_change_qid_priority(struct dlb2_hw *hw,
					      struct dlb2_ldb_port *port,
					      int slot,
					      struct dlb2_map_qid_args *args)
{
	u32 cq2priov;

	/* Read-modify-write the priority and valid bit register */
	cq2priov = DLB2_CSR_RD(hw,
			       DLB2_LSP_CQ2PRIOV(hw->ver, port->id.phys_id));

	cq2priov |= (1 << (slot + DLB2_LSP_CQ2PRIOV_V_LOC)) &
		    DLB2_LSP_CQ2PRIOV_V;
	cq2priov |= ((args->priority & 0x7) << slot * 3) &
		    DLB2_LSP_CQ2PRIOV_PRIO;

	DLB2_CSR_WR(hw, DLB2_LSP_CQ2PRIOV(hw->ver, port->id.phys_id), cq2priov);

	dlb2_flush_csr(hw);

	port->qid_map[slot].priority = args->priority;
}

static int dlb2_verify_map_qid_args(struct dlb2_hw *hw,
				    u32 domain_id,
				    struct dlb2_map_qid_args *args,
				    struct dlb2_cmd_response *resp,
				    bool vdev_req,
				    unsigned int vdev_id,
				    struct dlb2_hw_domain **out_domain,
				    struct dlb2_ldb_port **out_port,
				    struct dlb2_ldb_queue **out_queue)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_queue *queue;
	struct dlb2_ldb_port *port;
	int id;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	if (!domain->configured) {
		resp->status = DLB2_ST_DOMAIN_NOT_CONFIGURED;
		return -EINVAL;
	}

	id = args->port_id;

	port = dlb2_get_domain_used_ldb_port(id, vdev_req, domain);

	if (!port || !port->configured) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	if (args->priority >= DLB2_QID_PRIORITIES) {
		resp->status = DLB2_ST_INVALID_PRIORITY;
		return -EINVAL;
	}

	queue = dlb2_get_domain_ldb_queue(args->qid, vdev_req, domain);

	if (!queue || !queue->configured) {
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

	*out_domain = domain;
	*out_queue = queue;
	*out_port = port;

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

/**
 * dlb2_hw_map_qid() - map a load-balanced queue to a load-balanced port
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: map QID arguments.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function configures the DLB to schedule QEs from the specified queue
 * to the specified port. Each load-balanced port can be mapped to up to 8
 * queues; each load-balanced queue can potentially map to all the
 * load-balanced ports.
 *
 * A successful return does not necessarily mean the mapping was configured. If
 * this function is unable to immediately map the queue to the port, it will
 * add the requested operation to a per-port list of pending map/unmap
 * operations, and (if it's not already running) launch a kernel thread that
 * periodically attempts to process all pending operations. In a sense, this is
 * an asynchronous function.
 *
 * This asynchronicity creates two views of the state of hardware: the actual
 * hardware state and the requested state (as if every request completed
 * immediately). If there are any pending map/unmap operations, the requested
 * state will differ from the actual state. All validation is performed with
 * respect to the pending state; for instance, if there are 8 pending map
 * operations for port X, a request for a 9th will fail because a load-balanced
 * port can only map up to 8 queues.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, invalid port or queue ID, or
 *	    the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 * EBUSY  - The requested port has outstanding detach operations.
 */
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
	int ret, i;
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
				       vdev_id,
				       &domain,
				       &port,
				       &queue);
	if (ret)
		return ret;

	prio = args->priority;

	/*
	 * If there are any outstanding detach operations for this port,
	 * attempt to complete them. This may be necessary to free up a QID
	 * slot for this requested mapping.
	 */
	if (port->num_pending_removals) {
		bool bool_ret;
		bool_ret = dlb2_domain_finish_unmap_port(hw, domain, port);
		if (!bool_ret)
			return -EBUSY;
	}

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
		port->qid_map[i].priority = prio;

		DLB2_HW_DBG(hw, "DLB2 map: priority change only\n");

		goto map_qid_done;
	}

	/*
	 * If this is a priority change on a pending mapping, update the
	 * pending priority
	 */
	if (dlb2_port_find_slot_with_pending_map_queue(port, queue, &i)) {
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
			enum dlb2_qid_map_state new_st;

			port->qid_map[i].pending_qid = queue->id.phys_id;
			port->qid_map[i].pending_priority = prio;

			new_st = DLB2_QUEUE_UNMAP_IN_PROG_PENDING_MAP;

			ret = dlb2_port_slot_state_transition(hw, port, queue,
							      i, new_st);
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
				      unsigned int vdev_id,
				      struct dlb2_hw_domain **out_domain,
				      struct dlb2_ldb_port **out_port,
				      struct dlb2_ldb_queue **out_queue)
{
	enum dlb2_qid_map_state state;
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_queue *queue;
	struct dlb2_ldb_port *port;
	int slot;
	int id;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	if (!domain->configured) {
		resp->status = DLB2_ST_DOMAIN_NOT_CONFIGURED;
		return -EINVAL;
	}

	id = args->port_id;

	port = dlb2_get_domain_used_ldb_port(id, vdev_req, domain);

	if (!port || !port->configured) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	if (port->domain_id.phys_id != domain->id.phys_id) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	queue = dlb2_get_domain_ldb_queue(args->qid, vdev_req, domain);

	if (!queue || !queue->configured) {
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
		goto done;

	state = DLB2_QUEUE_MAP_IN_PROG;
	if (dlb2_port_find_slot_queue(port, state, queue, &slot))
		goto done;

	if (dlb2_port_find_slot_with_pending_map_queue(port, queue, &slot))
		goto done;

	resp->status = DLB2_ST_INVALID_QID;
	return -EINVAL;

done:
	*out_domain = domain;
	*out_port = port;
	*out_queue = queue;

	return 0;
}

/**
 * dlb2_hw_unmap_qid() - Unmap a load-balanced queue from a load-balanced port
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: unmap QID arguments.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function configures the DLB to stop scheduling QEs from the specified
 * queue to the specified port.
 *
 * A successful return does not necessarily mean the mapping was removed. If
 * this function is unable to immediately unmap the queue from the port, it
 * will add the requested operation to a per-port list of pending map/unmap
 * operations, and (if it's not already running) launch a kernel thread that
 * periodically attempts to process all pending operations. See
 * dlb2_hw_map_qid() for more details.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, invalid port or queue ID, or
 *	    the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
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
	int i, ret;

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
					 vdev_id,
					 &domain,
					 &port,
					 &queue);
	if (ret)
		return ret;

	/*
	 * If the queue hasn't been mapped yet, we need to update the slot's
	 * state and re-enable the queue's inflights.
	 */
	st = DLB2_QUEUE_MAP_IN_PROG;
	if (dlb2_port_find_slot_queue(port, st, queue, &i)) {
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

/**
 * dlb2_hw_pending_port_unmaps() - returns the number of unmap operations in
 *	progress.
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: number of unmaps in progress args
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the number of unmaps in progress.
 *
 * Errors:
 * EINVAL - Invalid port ID.
 */
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

	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	port = dlb2_get_domain_used_ldb_port(args->port_id, vdev_req, domain);
	if (!port || !port->configured) {
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
					 unsigned int vdev_id,
					 struct dlb2_hw_domain **out_domain)
{
	struct dlb2_hw_domain *domain;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
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

	*out_domain = domain;

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
 * dlb2_hw_start_domain() - start a scheduling domain
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @arg: start domain arguments.
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function starts a scheduling domain, which allows applications to send
 * traffic through it. Once a domain is started, its resources can no longer be
 * configured (besides QID remapping and port enable/disable).
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - the domain is not configured, or the domain is already started.
 */
int
dlb2_hw_start_domain(struct dlb2_hw *hw,
		     u32 domain_id,
		     struct dlb2_start_domain_args *args,
		     struct dlb2_cmd_response *resp,
		     bool vdev_req,
		     unsigned int vdev_id)
{
	struct dlb2_list_entry *iter;
	struct dlb2_dir_pq_pair *dir_queue;
	struct dlb2_ldb_queue *ldb_queue;
	struct dlb2_hw_domain *domain;
	int ret;
	RTE_SET_USED(args);
	RTE_SET_USED(iter);

	dlb2_log_start_domain(hw, domain_id, vdev_req, vdev_id);

	ret = dlb2_verify_start_domain_args(hw,
					    domain_id,
					    resp,
					    vdev_req,
					    vdev_id,
					    &domain);
	if (ret)
		return ret;

	/*
	 * Enable load-balanced and directed queue write permissions for the
	 * queues this domain owns. Without this, the DLB2 will drop all
	 * incoming traffic to those queues.
	 */
	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, ldb_queue, iter) {
		u32 vasqid_v = 0;
		unsigned int offs;

		DLB2_BIT_SET(vasqid_v, DLB2_SYS_LDB_VASQID_V_VASQID_V);

		offs = domain->id.phys_id * DLB2_MAX_NUM_LDB_QUEUES +
			ldb_queue->id.phys_id;

		DLB2_CSR_WR(hw, DLB2_SYS_LDB_VASQID_V(offs), vasqid_v);
	}

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, dir_queue, iter) {
		u32 vasqid_v = 0;
		unsigned int offs;

		DLB2_BIT_SET(vasqid_v, DLB2_SYS_DIR_VASQID_V_VASQID_V);

		offs = domain->id.phys_id * DLB2_MAX_NUM_DIR_PORTS(hw->ver) +
			dir_queue->id.phys_id;

		DLB2_CSR_WR(hw, DLB2_SYS_DIR_VASQID_V(offs), vasqid_v);
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

/**
 * dlb2_hw_get_dir_queue_depth() - returns the depth of a directed queue
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: queue depth args
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function returns the depth of a directed queue.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the depth.
 *
 * Errors:
 * EINVAL - Invalid domain ID or queue ID.
 */
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
	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	id = args->queue_id;

	queue = dlb2_get_domain_used_dir_pq(hw, id, vdev_req, domain);
	if (!queue) {
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

/**
 * dlb2_hw_get_ldb_queue_depth() - returns the depth of a load-balanced queue
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: queue depth args
 * @resp: response structure.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_req is true, this contains the vdev's ID.
 *
 * This function returns the depth of a load-balanced queue.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the depth.
 *
 * Errors:
 * EINVAL - Invalid domain ID or queue ID.
 */
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
	if (!domain) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	queue = dlb2_get_domain_ldb_queue(args->queue_id, vdev_req, domain);
	if (!queue) {
		resp->status = DLB2_ST_INVALID_QID;
		return -EINVAL;
	}

	resp->id = dlb2_ldb_queue_depth(hw, queue);

	return 0;
}

/**
 * dlb2_finish_unmap_qid_procedures() - finish any pending unmap procedures
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function attempts to finish any outstanding unmap procedures.
 * This function should be called by the kernel thread responsible for
 * finishing map/unmap procedures.
 *
 * Return:
 * Returns the number of procedures that weren't completed.
 */
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

/**
 * dlb2_finish_map_qid_procedures() - finish any pending map procedures
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function attempts to finish any outstanding map procedures.
 * This function should be called by the kernel thread responsible for
 * finishing map/unmap procedures.
 *
 * Return:
 * Returns the number of procedures that weren't completed.
 */
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

/**
 * dlb2_hw_enable_sparse_dir_cq_mode() - enable sparse mode for directed ports.
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function must be called prior to configuring scheduling domains.
 */

void dlb2_hw_enable_sparse_dir_cq_mode(struct dlb2_hw *hw)
{
	u32 ctrl;

	ctrl = DLB2_CSR_RD(hw, DLB2_CHP_CFG_CHP_CSR_CTRL);

	DLB2_BIT_SET(ctrl,
		     DLB2_CHP_CFG_CHP_CSR_CTRL_CFG_64BYTES_QE_DIR_CQ_MODE);

	DLB2_CSR_WR(hw, DLB2_CHP_CFG_CHP_CSR_CTRL, ctrl);
}

/**
 * dlb2_hw_enable_sparse_ldb_cq_mode() - enable sparse mode for load-balanced
 *	ports.
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function must be called prior to configuring scheduling domains.
 */
void dlb2_hw_enable_sparse_ldb_cq_mode(struct dlb2_hw *hw)
{
	u32 ctrl;

	ctrl = DLB2_CSR_RD(hw, DLB2_CHP_CFG_CHP_CSR_CTRL);

	DLB2_BIT_SET(ctrl,
		     DLB2_CHP_CFG_CHP_CSR_CTRL_CFG_64BYTES_QE_LDB_CQ_MODE);

	DLB2_CSR_WR(hw, DLB2_CHP_CFG_CHP_CSR_CTRL, ctrl);
}

/**
 * dlb2_get_group_sequence_numbers() - return a group's number of SNs per queue
 * @hw: dlb2_hw handle for a particular device.
 * @group_id: sequence number group ID.
 *
 * This function returns the configured number of sequence numbers per queue
 * for the specified group.
 *
 * Return:
 * Returns -EINVAL if group_id is invalid, else the group's SNs per queue.
 */
int dlb2_get_group_sequence_numbers(struct dlb2_hw *hw, u32 group_id)
{
	if (group_id >= DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS)
		return -EINVAL;

	return hw->rsrcs.sn_groups[group_id].sequence_numbers_per_queue;
}

/**
 * dlb2_get_group_sequence_number_occupancy() - return a group's in-use slots
 * @hw: dlb2_hw handle for a particular device.
 * @group_id: sequence number group ID.
 *
 * This function returns the group's number of in-use slots (i.e. load-balanced
 * queues using the specified group).
 *
 * Return:
 * Returns -EINVAL if group_id is invalid, else the group's SNs per queue.
 */
int dlb2_get_group_sequence_number_occupancy(struct dlb2_hw *hw, u32 group_id)
{
	if (group_id >= DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS)
		return -EINVAL;

	return dlb2_sn_group_used_slots(&hw->rsrcs.sn_groups[group_id]);
}

static void dlb2_log_set_group_sequence_numbers(struct dlb2_hw *hw,
						u32 group_id,
						u32 val)
{
	DLB2_HW_DBG(hw, "DLB2 set group sequence numbers:\n");
	DLB2_HW_DBG(hw, "\tGroup ID: %u\n", group_id);
	DLB2_HW_DBG(hw, "\tValue:    %u\n", val);
}

/**
 * dlb2_set_group_sequence_numbers() - assign a group's number of SNs per queue
 * @hw: dlb2_hw handle for a particular device.
 * @group_id: sequence number group ID.
 * @val: requested amount of sequence numbers per queue.
 *
 * This function configures the group's number of sequence numbers per queue.
 * val can be a power-of-two between 32 and 1024, inclusive. This setting can
 * be configured until the first ordered load-balanced queue is configured, at
 * which point the configuration is locked.
 *
 * Return:
 * Returns 0 upon success; -EINVAL if group_id or val is invalid, -EPERM if an
 * ordered queue is configured.
 */
int dlb2_set_group_sequence_numbers(struct dlb2_hw *hw,
				    u32 group_id,
				    u32 val)
{
	const u32 valid_allocations[] = {64, 128, 256, 512, 1024};
	struct dlb2_sn_group *group;
	u32 sn_mode = 0;
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

	DLB2_BITS_SET(sn_mode, hw->rsrcs.sn_groups[0].mode,
		 DLB2_RO_GRP_SN_MODE_SN_MODE_0);
	DLB2_BITS_SET(sn_mode, hw->rsrcs.sn_groups[1].mode,
		 DLB2_RO_GRP_SN_MODE_SN_MODE_1);

	DLB2_CSR_WR(hw, DLB2_RO_GRP_SN_MODE(hw->ver), sn_mode);

	dlb2_log_set_group_sequence_numbers(hw, group_id, val);

	return 0;
}

/**
 * dlb2_hw_set_qe_arbiter_weights() - program QE arbiter weights
 * @hw: dlb2_hw handle for a particular device.
 * @weight: 8-entry array of arbiter weights.
 *
 * weight[N] programs priority N's weight. In cases where the 8 priorities are
 * reduced to 4 bins, the mapping is:
 * - weight[1] programs bin 0
 * - weight[3] programs bin 1
 * - weight[5] programs bin 2
 * - weight[7] programs bin 3
 */
void dlb2_hw_set_qe_arbiter_weights(struct dlb2_hw *hw, u8 weight[8])
{
	u32 reg = 0;

	DLB2_BITS_SET(reg, weight[1], DLB2_ATM_CFG_ARB_WEIGHTS_RDY_BIN_BIN0);
	DLB2_BITS_SET(reg, weight[3], DLB2_ATM_CFG_ARB_WEIGHTS_RDY_BIN_BIN1);
	DLB2_BITS_SET(reg, weight[5], DLB2_ATM_CFG_ARB_WEIGHTS_RDY_BIN_BIN2);
	DLB2_BITS_SET(reg, weight[7], DLB2_ATM_CFG_ARB_WEIGHTS_RDY_BIN_BIN3);
	DLB2_CSR_WR(hw, DLB2_ATM_CFG_ARB_WEIGHTS_RDY_BIN, reg);

	reg = 0;
	DLB2_BITS_SET(reg, weight[1], DLB2_NALB_CFG_ARB_WEIGHTS_TQPRI_NALB_0_PRI0);
	DLB2_BITS_SET(reg, weight[3], DLB2_NALB_CFG_ARB_WEIGHTS_TQPRI_NALB_0_PRI1);
	DLB2_BITS_SET(reg, weight[5], DLB2_NALB_CFG_ARB_WEIGHTS_TQPRI_NALB_0_PRI2);
	DLB2_BITS_SET(reg, weight[7], DLB2_NALB_CFG_ARB_WEIGHTS_TQPRI_NALB_0_PRI3);
	DLB2_CSR_WR(hw, DLB2_NALB_CFG_ARB_WEIGHTS_TQPRI_NALB_0(hw->ver), reg);

	reg = 0;
	DLB2_BITS_SET(reg, weight[1], DLB2_NALB_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_PRI0);
	DLB2_BITS_SET(reg, weight[3], DLB2_NALB_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_PRI1);
	DLB2_BITS_SET(reg, weight[5], DLB2_NALB_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_PRI2);
	DLB2_BITS_SET(reg, weight[7], DLB2_NALB_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_PRI3);
	DLB2_CSR_WR(hw, DLB2_NALB_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0(hw->ver), reg);

	reg = 0;
	DLB2_BITS_SET(reg, weight[1], DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_PRI0);
	DLB2_BITS_SET(reg, weight[3], DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_PRI1);
	DLB2_BITS_SET(reg, weight[5], DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_PRI2);
	DLB2_BITS_SET(reg, weight[7], DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0_PRI3);
	DLB2_CSR_WR(hw, DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_REPLAY_0, reg);

	reg = 0;
	DLB2_BITS_SET(reg, weight[1], DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_DIR_0_PRI0);
	DLB2_BITS_SET(reg, weight[3], DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_DIR_0_PRI1);
	DLB2_BITS_SET(reg, weight[5], DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_DIR_0_PRI2);
	DLB2_BITS_SET(reg, weight[7], DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_DIR_0_PRI3);
	DLB2_CSR_WR(hw, DLB2_DP_CFG_ARB_WEIGHTS_TQPRI_DIR_0, reg);

	reg = 0;
	DLB2_BITS_SET(reg, weight[1], DLB2_NALB_CFG_ARB_WEIGHTS_TQPRI_ATQ_0_PRI0);
	DLB2_BITS_SET(reg, weight[3], DLB2_NALB_CFG_ARB_WEIGHTS_TQPRI_ATQ_0_PRI1);
	DLB2_BITS_SET(reg, weight[5], DLB2_NALB_CFG_ARB_WEIGHTS_TQPRI_ATQ_0_PRI2);
	DLB2_BITS_SET(reg, weight[7], DLB2_NALB_CFG_ARB_WEIGHTS_TQPRI_ATQ_0_PRI3);
	DLB2_CSR_WR(hw, DLB2_NALB_CFG_ARB_WEIGHTS_TQPRI_ATQ_0(hw->ver), reg);

	reg = 0;
	DLB2_BITS_SET(reg, weight[1], DLB2_ATM_CFG_ARB_WEIGHTS_SCHED_BIN_BIN0);
	DLB2_BITS_SET(reg, weight[3], DLB2_ATM_CFG_ARB_WEIGHTS_SCHED_BIN_BIN1);
	DLB2_BITS_SET(reg, weight[5], DLB2_ATM_CFG_ARB_WEIGHTS_SCHED_BIN_BIN2);
	DLB2_BITS_SET(reg, weight[7], DLB2_ATM_CFG_ARB_WEIGHTS_SCHED_BIN_BIN3);
	DLB2_CSR_WR(hw, DLB2_ATM_CFG_ARB_WEIGHTS_SCHED_BIN, reg);

	reg = 0;
	DLB2_BITS_SET(reg, weight[1], DLB2_AQED_CFG_ARB_WEIGHTS_TQPRI_ATM_0_PRI0);
	DLB2_BITS_SET(reg, weight[3], DLB2_AQED_CFG_ARB_WEIGHTS_TQPRI_ATM_0_PRI1);
	DLB2_BITS_SET(reg, weight[5], DLB2_AQED_CFG_ARB_WEIGHTS_TQPRI_ATM_0_PRI2);
	DLB2_BITS_SET(reg, weight[7], DLB2_AQED_CFG_ARB_WEIGHTS_TQPRI_ATM_0_PRI3);
	DLB2_CSR_WR(hw, DLB2_AQED_CFG_ARB_WEIGHTS_TQPRI_ATM_0, reg);
}

/**
 * dlb2_hw_set_qid_arbiter_weights() - program QID arbiter weights
 * @hw: dlb2_hw handle for a particular device.
 * @weight: 8-entry array of arbiter weights.
 *
 * weight[N] programs priority N's weight. In cases where the 8 priorities are
 * reduced to 4 bins, the mapping is:
 * - weight[1] programs bin 0
 * - weight[3] programs bin 1
 * - weight[5] programs bin 2
 * - weight[7] programs bin 3
 */
void dlb2_hw_set_qid_arbiter_weights(struct dlb2_hw *hw, u8 weight[8])
{
	u32 reg = 0;

	DLB2_BITS_SET(reg, weight[1], DLB2_LSP_CFG_ARB_WEIGHT_LDB_QID_0_PRI0_WEIGHT);
	DLB2_BITS_SET(reg, weight[3], DLB2_LSP_CFG_ARB_WEIGHT_LDB_QID_0_PRI1_WEIGHT);
	DLB2_BITS_SET(reg, weight[5], DLB2_LSP_CFG_ARB_WEIGHT_LDB_QID_0_PRI2_WEIGHT);
	DLB2_BITS_SET(reg, weight[7], DLB2_LSP_CFG_ARB_WEIGHT_LDB_QID_0_PRI3_WEIGHT);
	DLB2_CSR_WR(hw, DLB2_LSP_CFG_ARB_WEIGHT_LDB_QID_0(hw->ver), reg);

	reg = 0;
	DLB2_BITS_SET(reg, weight[1], DLB2_LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_0_PRI0_WEIGHT);
	DLB2_BITS_SET(reg, weight[3], DLB2_LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_0_PRI1_WEIGHT);
	DLB2_BITS_SET(reg, weight[5], DLB2_LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_0_PRI2_WEIGHT);
	DLB2_BITS_SET(reg, weight[7], DLB2_LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_0_PRI3_WEIGHT);
	DLB2_CSR_WR(hw, DLB2_LSP_CFG_ARB_WEIGHT_ATM_NALB_QID_0(hw->ver), reg);
}

static void dlb2_log_enable_cq_weight(struct dlb2_hw *hw,
				      u32 domain_id,
				      struct dlb2_enable_cq_weight_args *args,
				      bool vdev_req,
				      unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 enable CQ weight arguments:\n");
	DLB2_HW_DBG(hw, "\tvdev_req %d, vdev_id %d\n", vdev_req, vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n", domain_id);
	DLB2_HW_DBG(hw, "\tPort ID:   %d\n", args->port_id);
	DLB2_HW_DBG(hw, "\tLimit:   %d\n", args->limit);
}

static int
dlb2_verify_enable_cq_weight_args(struct dlb2_hw *hw,
				  u32 domain_id,
				  struct dlb2_enable_cq_weight_args *args,
				  struct dlb2_cmd_response *resp,
				  bool vdev_req,
				  unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;

	if (hw->ver == DLB2_HW_V2) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] CQ weight feature requires DLB 2.5 or later\n",
			    __func__, __LINE__);
		resp->status = DLB2_ST_FEATURE_UNAVAILABLE;
		return -EINVAL;
	}

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (!domain) {
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

	port = dlb2_get_domain_used_ldb_port(args->port_id, vdev_req, domain);
	if (!port || !port->configured) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	if (args->limit == 0 || args->limit > port->cq_depth) {
		resp->status = DLB2_ST_INVALID_CQ_WEIGHT_LIMIT;
		return -EINVAL;
	}

	return 0;
}

int dlb2_hw_enable_cq_weight(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_enable_cq_weight_args *args,
			     struct dlb2_cmd_response *resp,
			     bool vdev_req,
			     unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;
	int ret, id;
	u32 reg = 0;

	dlb2_log_enable_cq_weight(hw, domain_id, args, vdev_req, vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_enable_cq_weight_args(hw,
						domain_id,
						args,
						resp,
						vdev_req,
						vdev_id);
	if (ret)
		return ret;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);
	if (!domain) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: domain not found\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	id = args->port_id;

	port = dlb2_get_domain_used_ldb_port(id, vdev_req, domain);
	if (!port) {
		DLB2_HW_ERR(hw,
			    "[%s():	%d] Internal error: port not found\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	DLB2_BIT_SET(reg, DLB2_LSP_CFG_CQ_LDB_WU_LIMIT_V);
	DLB2_BITS_SET(reg, args->limit, DLB2_LSP_CFG_CQ_LDB_WU_LIMIT_LIMIT);

	DLB2_CSR_WR(hw, DLB2_LSP_CFG_CQ_LDB_WU_LIMIT(port->id.phys_id), reg);

	resp->status = 0;

	return 0;
}

static void dlb2_log_set_cos_bandwidth(struct dlb2_hw *hw, u32 cos_id, u8 bw)
{
	DLB2_HW_DBG(hw, "DLB2 set port CoS bandwidth:\n");
	DLB2_HW_DBG(hw, "\tCoS ID:    %u\n", cos_id);
	DLB2_HW_DBG(hw, "\tBandwidth: %u\n", bw);
}

#define DLB2_MAX_BW_PCT 100

/**
 * dlb2_hw_set_cos_bandwidth() - set a bandwidth allocation percentage for a
 *      port class-of-service.
 * @hw: dlb2_hw handle for a particular device.
 * @cos_id: class-of-service ID.
 * @bandwidth: class-of-service bandwidth.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - Invalid cos ID, bandwidth is greater than 100, or bandwidth would
 *          cause the total bandwidth across all classes of service to exceed
 *          100%.
 */
int dlb2_hw_set_cos_bandwidth(struct dlb2_hw *hw, u32 cos_id, u8 bandwidth)
{
	unsigned int i;
	u32 reg;
	u8 total;

	if (cos_id >= DLB2_NUM_COS_DOMAINS)
		return -EINVAL;

	if (bandwidth > DLB2_MAX_BW_PCT)
		return -EINVAL;

	total = 0;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
		total += (i == cos_id) ? bandwidth : hw->cos_reservation[i];

	if (total > DLB2_MAX_BW_PCT)
		return -EINVAL;

	reg = DLB2_CSR_RD(hw, DLB2_LSP_CFG_SHDW_RANGE_COS(hw->ver, cos_id));

	/*
	 * Normalize the bandwidth to a value in the range 0-255. Integer
	 * division may leave unreserved scheduling slots; these will be
	 * divided among the 4 classes of service.
	 */
	DLB2_BITS_SET(reg, (bandwidth * 256) / 100, DLB2_LSP_CFG_SHDW_RANGE_COS_BW_RANGE);
	DLB2_CSR_WR(hw, DLB2_LSP_CFG_SHDW_RANGE_COS(hw->ver, cos_id), reg);

	reg = 0;
	DLB2_BIT_SET(reg, DLB2_LSP_CFG_SHDW_CTRL_TRANSFER);
	/* Atomically transfer the newly configured service weight */
	DLB2_CSR_WR(hw, DLB2_LSP_CFG_SHDW_CTRL(hw->ver), reg);

	dlb2_log_set_cos_bandwidth(hw, cos_id, bandwidth);

	hw->cos_reservation[cos_id] = bandwidth;

	return 0;
}
