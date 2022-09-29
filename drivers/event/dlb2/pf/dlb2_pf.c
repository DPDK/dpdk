/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include <rte_debug.h>
#include <rte_log.h>
#include <dev_driver.h>
#include <rte_devargs.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_errno.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_io.h>
#include <rte_pci.h>
#include <bus_pci_driver.h>
#include <rte_eventdev.h>
#include <eventdev_pmd.h>
#include <eventdev_pmd_pci.h>
#include <rte_memory.h>
#include <rte_string_fns.h>

#include "../dlb2_priv.h"
#include "../dlb2_iface.h"
#include "../dlb2_inline_fns.h"
#include "dlb2_main.h"
#include "base/dlb2_hw_types.h"
#include "base/dlb2_osdep.h"
#include "base/dlb2_resource.h"

static const char *event_dlb2_pf_name = RTE_STR(EVDEV_DLB2_NAME_PMD);
static unsigned int dlb2_qe_sa_pct = 1;
static unsigned int dlb2_qid_sa_pct;

static void
dlb2_pf_low_level_io_init(void)
{
	int i;
	/* Addresses will be initialized at port create */
	for (i = 0; i < DLB2_MAX_NUM_PORTS(DLB2_HW_V2_5); i++) {
		/* First directed ports */
		dlb2_port[i][DLB2_DIR_PORT].pp_addr = NULL;
		dlb2_port[i][DLB2_DIR_PORT].cq_base = NULL;
		dlb2_port[i][DLB2_DIR_PORT].mmaped = true;

		/* Now load balanced ports */
		dlb2_port[i][DLB2_LDB_PORT].pp_addr = NULL;
		dlb2_port[i][DLB2_LDB_PORT].cq_base = NULL;
		dlb2_port[i][DLB2_LDB_PORT].mmaped = true;
	}
}

static int
dlb2_pf_open(struct dlb2_hw_dev *handle, const char *name)
{
	RTE_SET_USED(handle);
	RTE_SET_USED(name);

	return 0;
}

static int
dlb2_pf_get_device_version(struct dlb2_hw_dev *handle,
			   uint8_t *revision)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;

	*revision = dlb2_dev->revision;

	return 0;
}

static void dlb2_pf_calc_arbiter_weights(u8 *weight,
					 unsigned int pct)
{
	int val, i;

	/* Largest possible weight (100% SA case): 32 */
	val = (DLB2_MAX_WEIGHT + 1) / DLB2_NUM_ARB_WEIGHTS;

	/* Scale val according to the starvation avoidance percentage */
	val = (val * pct) / 100;
	if (val == 0 && pct != 0)
		val = 1;

	/* Prio 7 always has weight 0xff */
	weight[DLB2_NUM_ARB_WEIGHTS - 1] = DLB2_MAX_WEIGHT;

	for (i = DLB2_NUM_ARB_WEIGHTS - 2; i >= 0; i--)
		weight[i] = weight[i + 1] - val;
}


static void
dlb2_pf_hardware_init(struct dlb2_hw_dev *handle)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;

	dlb2_hw_enable_sparse_ldb_cq_mode(&dlb2_dev->hw);
	dlb2_hw_enable_sparse_dir_cq_mode(&dlb2_dev->hw);

	/* Configure arbitration weights for QE selection */
	if (dlb2_qe_sa_pct <= 100) {
		u8 weight[DLB2_NUM_ARB_WEIGHTS];

		dlb2_pf_calc_arbiter_weights(weight,
					     dlb2_qe_sa_pct);

		dlb2_hw_set_qe_arbiter_weights(&dlb2_dev->hw, weight);
	}

	/* Configure arbitration weights for QID selection */
	if (dlb2_qid_sa_pct <= 100) {
		u8 weight[DLB2_NUM_ARB_WEIGHTS];

		dlb2_pf_calc_arbiter_weights(weight,
					     dlb2_qid_sa_pct);

		dlb2_hw_set_qid_arbiter_weights(&dlb2_dev->hw, weight);
	}

}

static int
dlb2_pf_get_num_resources(struct dlb2_hw_dev *handle,
			  struct dlb2_get_num_resources_args *rsrcs)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;

	return dlb2_hw_get_num_resources(&dlb2_dev->hw, rsrcs, false, 0);
}

static int
dlb2_pf_get_cq_poll_mode(struct dlb2_hw_dev *handle,
			 enum dlb2_cq_poll_modes *mode)
{
	RTE_SET_USED(handle);

	*mode = DLB2_CQ_POLL_MODE_SPARSE;

	return 0;
}

static int
dlb2_pf_sched_domain_create(struct dlb2_hw_dev *handle,
			    struct dlb2_create_sched_domain_args *arg)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;
	struct dlb2_cmd_response response = {0};
	int ret;

	DLB2_INFO(dev->dlb2_device, "Entering %s()\n", __func__);

	if (dlb2_dev->domain_reset_failed) {
		response.status = DLB2_ST_DOMAIN_RESET_FAILED;
		ret = -EINVAL;
		goto done;
	}

	ret = dlb2_pf_create_sched_domain(&dlb2_dev->hw, arg, &response);
	if (ret)
		goto done;

done:

	arg->response = response;

	DLB2_INFO(dev->dlb2_device, "Exiting %s() with ret=%d\n",
		  __func__, ret);

	return ret;
}

static void
dlb2_pf_domain_reset(struct dlb2_eventdev *dlb2)
{
	struct dlb2_dev *dlb2_dev;
	int ret;

	dlb2_dev = (struct dlb2_dev *)dlb2->qm_instance.pf_dev;
	ret = dlb2_pf_reset_domain(&dlb2_dev->hw, dlb2->qm_instance.domain_id);
	if (ret)
		DLB2_LOG_ERR("dlb2_pf_reset_domain err %d", ret);
}

static int
dlb2_pf_ldb_queue_create(struct dlb2_hw_dev *handle,
			 struct dlb2_create_ldb_queue_args *cfg)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;
	struct dlb2_cmd_response response = {0};
	int ret;

	DLB2_INFO(dev->dlb2_device, "Entering %s()\n", __func__);

	ret = dlb2_pf_create_ldb_queue(&dlb2_dev->hw,
				       handle->domain_id,
				       cfg,
				       &response);

	cfg->response = response;

	DLB2_INFO(dev->dlb2_device, "Exiting %s() with ret=%d\n",
		  __func__, ret);

	return ret;
}

static int
dlb2_pf_get_sn_occupancy(struct dlb2_hw_dev *handle,
			 struct dlb2_get_sn_occupancy_args *args)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;
	struct dlb2_cmd_response response = {0};
	int ret;

	ret = dlb2_get_group_sequence_number_occupancy(&dlb2_dev->hw,
						       args->group);

	response.id = ret;
	response.status = 0;

	args->response = response;

	return ret;
}

static int
dlb2_pf_get_sn_allocation(struct dlb2_hw_dev *handle,
			  struct dlb2_get_sn_allocation_args *args)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;
	struct dlb2_cmd_response response = {0};
	int ret;

	ret = dlb2_get_group_sequence_numbers(&dlb2_dev->hw, args->group);

	response.id = ret;
	response.status = 0;

	args->response = response;

	return ret;
}

static int
dlb2_pf_set_sn_allocation(struct dlb2_hw_dev *handle,
			  struct dlb2_set_sn_allocation_args *args)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;
	struct dlb2_cmd_response response = {0};
	int ret;

	ret = dlb2_set_group_sequence_numbers(&dlb2_dev->hw, args->group,
					      args->num);

	response.status = 0;

	args->response = response;

	return ret;
}

static void *
dlb2_alloc_coherent_aligned(const struct rte_memzone **mz, uintptr_t *phys,
			    size_t size, int align)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	uint32_t core_id = rte_lcore_id();
	unsigned int socket_id;

	snprintf(mz_name, sizeof(mz_name) - 1, "event_dlb2_pf_%lx",
		 (unsigned long)rte_get_timer_cycles());
	if (core_id == (unsigned int)LCORE_ID_ANY)
		core_id = rte_get_main_lcore();
	socket_id = rte_lcore_to_socket_id(core_id);
	*mz = rte_memzone_reserve_aligned(mz_name, size, socket_id,
					 RTE_MEMZONE_IOVA_CONTIG, align);
	if (*mz == NULL) {
		DLB2_LOG_DBG("Unable to allocate DMA memory of size %zu bytes - %s\n",
			     size, rte_strerror(rte_errno));
		*phys = 0;
		return NULL;
	}
	*phys = (*mz)->iova;
	return (*mz)->addr;
}

static int
dlb2_pf_ldb_port_create(struct dlb2_hw_dev *handle,
			struct dlb2_create_ldb_port_args *cfg,
			enum dlb2_cq_poll_modes poll_mode)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;
	struct dlb2_cmd_response response = {0};
	struct dlb2_port_memory port_memory;
	int ret, cq_alloc_depth;
	uint8_t *port_base;
	const struct rte_memzone *mz;
	int alloc_sz, qe_sz;
	phys_addr_t cq_base;
	phys_addr_t pp_base;
	int is_dir = false;

	DLB2_INFO(dev->dlb2_device, "Entering %s()\n", __func__);

	if (poll_mode == DLB2_CQ_POLL_MODE_STD)
		qe_sz = sizeof(struct dlb2_dequeue_qe);
	else
		qe_sz = RTE_CACHE_LINE_SIZE;

	/* Calculate the port memory required, and round up to the nearest
	 * cache line.
	 */
	cq_alloc_depth = RTE_MAX(cfg->cq_depth, DLB2_MIN_HARDWARE_CQ_DEPTH);
	alloc_sz = cq_alloc_depth * qe_sz;
	alloc_sz = RTE_CACHE_LINE_ROUNDUP(alloc_sz);

	port_base = dlb2_alloc_coherent_aligned(&mz, &cq_base, alloc_sz,
						rte_mem_page_size());
	if (port_base == NULL)
		return -ENOMEM;

	/* Lock the page in memory */
	ret = rte_mem_lock_page(port_base);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2 pf pmd could not lock page for device i/o\n");
		goto create_port_err;
	}

	memset(port_base, 0, alloc_sz);

	ret = dlb2_pf_create_ldb_port(&dlb2_dev->hw,
				      handle->domain_id,
				      cfg,
				      cq_base,
				      &response);
	if (ret)
		goto create_port_err;

	pp_base = (uintptr_t)dlb2_dev->hw.func_kva + PP_BASE(is_dir);
	dlb2_port[response.id][DLB2_LDB_PORT].pp_addr =
		(void *)(pp_base + (rte_mem_page_size() * response.id));

	dlb2_port[response.id][DLB2_LDB_PORT].cq_base = (void *)(port_base);
	memset(&port_memory, 0, sizeof(port_memory));

	dlb2_port[response.id][DLB2_LDB_PORT].mz = mz;

	dlb2_list_init_head(&port_memory.list);

	cfg->response = response;

	return 0;

create_port_err:

	rte_memzone_free(mz);

	DLB2_INFO(dev->dlb2_device, "Exiting %s() with ret=%d\n",
		  __func__, ret);
	return ret;
}

static int
dlb2_pf_dir_port_create(struct dlb2_hw_dev *handle,
			struct dlb2_create_dir_port_args *cfg,
			enum dlb2_cq_poll_modes poll_mode)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;
	struct dlb2_cmd_response response = {0};
	struct dlb2_port_memory port_memory;
	int ret;
	uint8_t *port_base;
	const struct rte_memzone *mz;
	int alloc_sz, qe_sz;
	phys_addr_t cq_base;
	phys_addr_t pp_base;
	int is_dir = true;

	DLB2_INFO(dev->dlb2_device, "Entering %s()\n", __func__);

	if (poll_mode == DLB2_CQ_POLL_MODE_STD)
		qe_sz = sizeof(struct dlb2_dequeue_qe);
	else
		qe_sz = RTE_CACHE_LINE_SIZE;

	/* Calculate the port memory required, and round up to the nearest
	 * cache line.
	 */
	alloc_sz = cfg->cq_depth * qe_sz;
	alloc_sz = RTE_CACHE_LINE_ROUNDUP(alloc_sz);

	port_base = dlb2_alloc_coherent_aligned(&mz, &cq_base, alloc_sz,
						rte_mem_page_size());
	if (port_base == NULL)
		return -ENOMEM;

	/* Lock the page in memory */
	ret = rte_mem_lock_page(port_base);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2 pf pmd could not lock page for device i/o\n");
		goto create_port_err;
	}

	memset(port_base, 0, alloc_sz);

	ret = dlb2_pf_create_dir_port(&dlb2_dev->hw,
				      handle->domain_id,
				      cfg,
				      cq_base,
				      &response);
	if (ret)
		goto create_port_err;

	pp_base = (uintptr_t)dlb2_dev->hw.func_kva + PP_BASE(is_dir);
	dlb2_port[response.id][DLB2_DIR_PORT].pp_addr =
		(void *)(pp_base + (rte_mem_page_size() * response.id));

	dlb2_port[response.id][DLB2_DIR_PORT].cq_base =
		(void *)(port_base);
	memset(&port_memory, 0, sizeof(port_memory));

	dlb2_port[response.id][DLB2_DIR_PORT].mz = mz;

	dlb2_list_init_head(&port_memory.list);

	cfg->response = response;

	return 0;

create_port_err:

	rte_memzone_free(mz);

	DLB2_INFO(dev->dlb2_device, "Exiting %s() with ret=%d\n",
		  __func__, ret);

	return ret;
}

static int
dlb2_pf_dir_queue_create(struct dlb2_hw_dev *handle,
			 struct dlb2_create_dir_queue_args *cfg)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;
	struct dlb2_cmd_response response = {0};
	int ret;

	DLB2_INFO(dev->dlb2_device, "Entering %s()\n", __func__);

	ret = dlb2_pf_create_dir_queue(&dlb2_dev->hw,
				       handle->domain_id,
				       cfg,
				       &response);

	cfg->response = response;

	DLB2_INFO(dev->dlb2_device, "Exiting %s() with ret=%d\n",
		  __func__, ret);

	return ret;
}

static int
dlb2_pf_map_qid(struct dlb2_hw_dev *handle,
		struct dlb2_map_qid_args *cfg)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;
	struct dlb2_cmd_response response = {0};
	int ret;

	DLB2_INFO(dev->dlb2_device, "Entering %s()\n", __func__);

	ret = dlb2_hw_map_qid(&dlb2_dev->hw,
			      handle->domain_id,
			      cfg,
			      &response,
			      false,
			      0);

	cfg->response = response;

	DLB2_INFO(dev->dlb2_device, "Exiting %s() with ret=%d\n",
		  __func__, ret);

	return ret;
}

static int
dlb2_pf_unmap_qid(struct dlb2_hw_dev *handle,
		  struct dlb2_unmap_qid_args *cfg)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;
	struct dlb2_cmd_response response = {0};
	int ret;

	DLB2_INFO(dev->dlb2_device, "Entering %s()\n", __func__);

	ret = dlb2_hw_unmap_qid(&dlb2_dev->hw,
				handle->domain_id,
				cfg,
				&response,
				false,
				0);

	cfg->response = response;

	DLB2_INFO(dev->dlb2_device, "Exiting %s() with ret=%d\n",
		  __func__, ret);

	return ret;
}

static int
dlb2_pf_pending_port_unmaps(struct dlb2_hw_dev *handle,
			    struct dlb2_pending_port_unmaps_args *args)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;
	struct dlb2_cmd_response response = {0};
	int ret;

	DLB2_INFO(dev->dlb2_device, "Entering %s()\n", __func__);

	ret = dlb2_hw_pending_port_unmaps(&dlb2_dev->hw,
					  handle->domain_id,
					  args,
					  &response,
					  false,
					  0);

	args->response = response;

	DLB2_INFO(dev->dlb2_device, "Exiting %s() with ret=%d\n",
		  __func__, ret);

	return ret;
}

static int
dlb2_pf_sched_domain_start(struct dlb2_hw_dev *handle,
			   struct dlb2_start_domain_args *cfg)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;
	struct dlb2_cmd_response response = {0};
	int ret;

	DLB2_INFO(dev->dlb2_device, "Entering %s()\n", __func__);

	ret = dlb2_pf_start_domain(&dlb2_dev->hw,
				   handle->domain_id,
				   cfg,
				   &response);

	cfg->response = response;

	DLB2_INFO(dev->dlb2_device, "Exiting %s() with ret=%d\n",
		  __func__, ret);

	return ret;
}

static int
dlb2_pf_get_ldb_queue_depth(struct dlb2_hw_dev *handle,
			    struct dlb2_get_ldb_queue_depth_args *args)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;
	struct dlb2_cmd_response response = {0};
	int ret;

	DLB2_INFO(dev->dlb2_device, "Entering %s()\n", __func__);

	ret = dlb2_hw_get_ldb_queue_depth(&dlb2_dev->hw,
					  handle->domain_id,
					  args,
					  &response,
					  false,
					  0);

	args->response = response;

	DLB2_INFO(dev->dlb2_device, "Exiting %s() with ret=%d\n",
		  __func__, ret);

	return ret;
}

static int
dlb2_pf_get_dir_queue_depth(struct dlb2_hw_dev *handle,
			    struct dlb2_get_dir_queue_depth_args *args)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;
	struct dlb2_cmd_response response = {0};
	int ret = 0;

	DLB2_INFO(dev->dlb2_device, "Entering %s()\n", __func__);

	ret = dlb2_hw_get_dir_queue_depth(&dlb2_dev->hw,
					  handle->domain_id,
					  args,
					  &response,
					  false,
					  0);

	args->response = response;

	DLB2_INFO(dev->dlb2_device, "Exiting %s() with ret=%d\n",
		  __func__, ret);

	return ret;
}

static int
dlb2_pf_enable_cq_weight(struct dlb2_hw_dev *handle,
			 struct dlb2_enable_cq_weight_args *args)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;
	struct dlb2_cmd_response response = {0};
	int ret = 0;

	DLB2_INFO(dev->dlb2_device, "Entering %s()\n", __func__);

	ret = dlb2_hw_enable_cq_weight(&dlb2_dev->hw,
				       handle->domain_id,
				       args,
				       &response,
				       false,
				       0);
	args->response = response;

	DLB2_INFO(dev->dlb2_device, "Exiting %s() with ret=%d\n",
		  __func__, ret);

	return ret;
}

static int
dlb2_pf_set_cos_bandwidth(struct dlb2_hw_dev *handle,
			  struct dlb2_set_cos_bw_args *args)
{
	struct dlb2_dev *dlb2_dev = (struct dlb2_dev *)handle->pf_dev;
	int ret = 0;

	DLB2_INFO(dev->dlb2_device, "Entering %s()\n", __func__);

	ret = dlb2_hw_set_cos_bandwidth(&dlb2_dev->hw,
					args->cos_id,
					args->bandwidth);

	DLB2_INFO(dev->dlb2_device, "Exiting %s() with ret=%d\n",
		  __func__, ret);

	return ret;
}

static void
dlb2_pf_iface_fn_ptrs_init(void)
{
	dlb2_iface_low_level_io_init = dlb2_pf_low_level_io_init;
	dlb2_iface_open = dlb2_pf_open;
	dlb2_iface_domain_reset = dlb2_pf_domain_reset;
	dlb2_iface_get_device_version = dlb2_pf_get_device_version;
	dlb2_iface_hardware_init = dlb2_pf_hardware_init;
	dlb2_iface_get_num_resources = dlb2_pf_get_num_resources;
	dlb2_iface_get_cq_poll_mode = dlb2_pf_get_cq_poll_mode;
	dlb2_iface_sched_domain_create = dlb2_pf_sched_domain_create;
	dlb2_iface_ldb_queue_create = dlb2_pf_ldb_queue_create;
	dlb2_iface_ldb_port_create = dlb2_pf_ldb_port_create;
	dlb2_iface_dir_queue_create = dlb2_pf_dir_queue_create;
	dlb2_iface_dir_port_create = dlb2_pf_dir_port_create;
	dlb2_iface_map_qid = dlb2_pf_map_qid;
	dlb2_iface_unmap_qid = dlb2_pf_unmap_qid;
	dlb2_iface_get_ldb_queue_depth = dlb2_pf_get_ldb_queue_depth;
	dlb2_iface_get_dir_queue_depth = dlb2_pf_get_dir_queue_depth;
	dlb2_iface_sched_domain_start = dlb2_pf_sched_domain_start;
	dlb2_iface_pending_port_unmaps = dlb2_pf_pending_port_unmaps;
	dlb2_iface_get_sn_allocation = dlb2_pf_get_sn_allocation;
	dlb2_iface_set_sn_allocation = dlb2_pf_set_sn_allocation;
	dlb2_iface_get_sn_occupancy = dlb2_pf_get_sn_occupancy;
	dlb2_iface_enable_cq_weight = dlb2_pf_enable_cq_weight;
	dlb2_iface_set_cos_bw = dlb2_pf_set_cos_bandwidth;
}

/* PCI DEV HOOKS */
static int
dlb2_eventdev_pci_init(struct rte_eventdev *eventdev)
{
	int ret = 0;
	struct rte_pci_device *pci_dev;
	struct dlb2_devargs dlb2_args = {
		.socket_id = rte_socket_id(),
		.max_num_events = DLB2_MAX_NUM_LDB_CREDITS,
		.producer_coremask = NULL,
		.num_dir_credits_override = -1,
		.qid_depth_thresholds = { {0} },
		.poll_interval = DLB2_POLL_INTERVAL_DEFAULT,
		.sw_credit_quanta = DLB2_SW_CREDIT_QUANTA_DEFAULT,
		.hw_credit_quanta = DLB2_SW_CREDIT_BATCH_SZ,
		.default_depth_thresh = DLB2_DEPTH_THRESH_DEFAULT,
		.max_cq_depth = DLB2_DEFAULT_CQ_DEPTH,
		.max_enq_depth = DLB2_MAX_ENQUEUE_DEPTH
	};
	struct dlb2_eventdev *dlb2;
	int q;
	const void *probe_args = NULL;

	DLB2_LOG_DBG("Enter with dev_id=%d socket_id=%d",
		     eventdev->data->dev_id, eventdev->data->socket_id);

	for (q = 0; q < DLB2_MAX_NUM_PORTS_ALL; q++)
		dlb2_args.port_cos.cos_id[q] = DLB2_COS_DEFAULT;

	dlb2_pf_iface_fn_ptrs_init();

	pci_dev = RTE_DEV_TO_PCI(eventdev->dev);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		dlb2 = dlb2_pmd_priv(eventdev); /* rte_zmalloc_socket mem */
		dlb2->version = DLB2_HW_DEVICE_FROM_PCI_ID(pci_dev);

		/* Were we invoked with runtime parameters? */
		if (pci_dev->device.devargs) {
			ret = dlb2_parse_params(pci_dev->device.devargs->args,
						pci_dev->device.devargs->name,
						&dlb2_args,
						dlb2->version);
			if (ret) {
				DLB2_LOG_ERR("PFPMD failed to parse args ret=%d, errno=%d\n",
					     ret, rte_errno);
				goto dlb2_probe_failed;
			}
			probe_args = &dlb2_args;
		}

		/* Probe the DLB2 PF layer */
		dlb2->qm_instance.pf_dev = dlb2_probe(pci_dev, probe_args);

		if (dlb2->qm_instance.pf_dev == NULL) {
			DLB2_LOG_ERR("DLB2 PF Probe failed with error %d\n",
				     rte_errno);
			ret = -rte_errno;
			goto dlb2_probe_failed;
		}

		ret = dlb2_primary_eventdev_probe(eventdev,
						  event_dlb2_pf_name,
						  &dlb2_args);
	} else {
		dlb2 = dlb2_pmd_priv(eventdev);
		dlb2->version = DLB2_HW_DEVICE_FROM_PCI_ID(pci_dev);
		ret = dlb2_secondary_eventdev_probe(eventdev,
						    event_dlb2_pf_name);
	}
	if (ret)
		goto dlb2_probe_failed;

	DLB2_LOG_INFO("DLB2 PF Probe success\n");

	return 0;

dlb2_probe_failed:

	DLB2_LOG_INFO("DLB2 PF Probe failed, ret=%d\n", ret);

	return ret;
}

#define EVENTDEV_INTEL_VENDOR_ID 0x8086

static const struct rte_pci_id pci_id_dlb2_map[] = {
	{
		RTE_PCI_DEVICE(EVENTDEV_INTEL_VENDOR_ID,
			       PCI_DEVICE_ID_INTEL_DLB2_PF)
	},
	{
		.vendor_id = 0,
	},
};

static const struct rte_pci_id pci_id_dlb2_5_map[] = {
	{
		RTE_PCI_DEVICE(EVENTDEV_INTEL_VENDOR_ID,
			       PCI_DEVICE_ID_INTEL_DLB2_5_PF)
	},
	{
		.vendor_id = 0,
	},
};

static int
event_dlb2_pci_probe(struct rte_pci_driver *pci_drv,
		     struct rte_pci_device *pci_dev)
{
	int ret;

	ret = rte_event_pmd_pci_probe_named(pci_drv, pci_dev,
					     sizeof(struct dlb2_eventdev),
					     dlb2_eventdev_pci_init,
					     event_dlb2_pf_name);
	if (ret) {
		DLB2_LOG_INFO("rte_event_pmd_pci_probe_named() failed, "
				"ret=%d\n", ret);
	}

	return ret;
}

static int
event_dlb2_pci_remove(struct rte_pci_device *pci_dev)
{
	int ret;

	ret = rte_event_pmd_pci_remove(pci_dev, NULL);

	if (ret) {
		DLB2_LOG_INFO("rte_event_pmd_pci_remove() failed, "
				"ret=%d\n", ret);
	}

	return ret;

}

static int
event_dlb2_5_pci_probe(struct rte_pci_driver *pci_drv,
		       struct rte_pci_device *pci_dev)
{
	int ret;

	ret = rte_event_pmd_pci_probe_named(pci_drv, pci_dev,
					    sizeof(struct dlb2_eventdev),
					    dlb2_eventdev_pci_init,
					    event_dlb2_pf_name);
	if (ret) {
		DLB2_LOG_INFO("rte_event_pmd_pci_probe_named() failed, "
				"ret=%d\n", ret);
	}

	return ret;
}

static int
event_dlb2_5_pci_remove(struct rte_pci_device *pci_dev)
{
	int ret;

	ret = rte_event_pmd_pci_remove(pci_dev, NULL);

	if (ret) {
		DLB2_LOG_INFO("rte_event_pmd_pci_remove() failed, "
				"ret=%d\n", ret);
	}

	return ret;

}

static struct rte_pci_driver pci_eventdev_dlb2_pmd = {
	.id_table = pci_id_dlb2_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = event_dlb2_pci_probe,
	.remove = event_dlb2_pci_remove,
};

static struct rte_pci_driver pci_eventdev_dlb2_5_pmd = {
	.id_table = pci_id_dlb2_5_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = event_dlb2_5_pci_probe,
	.remove = event_dlb2_5_pci_remove,
};

RTE_PMD_REGISTER_PCI(event_dlb2_pf, pci_eventdev_dlb2_pmd);
RTE_PMD_REGISTER_PCI_TABLE(event_dlb2_pf, pci_id_dlb2_map);

RTE_PMD_REGISTER_PCI(event_dlb2_5_pf, pci_eventdev_dlb2_5_pmd);
RTE_PMD_REGISTER_PCI_TABLE(event_dlb2_5_pf, pci_id_dlb2_5_map);
