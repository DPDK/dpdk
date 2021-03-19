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
#include <rte_dev.h>
#include <rte_devargs.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_errno.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_io.h>
#include <rte_memory.h>
#include <rte_string_fns.h>

#include "../dlb_priv.h"
#include "../dlb_iface.h"
#include "../dlb_inline_fns.h"
#include "dlb_main.h"
#include "base/dlb_hw_types.h"
#include "base/dlb_osdep.h"
#include "base/dlb_resource.h"

static void
dlb_pf_low_level_io_init(struct dlb_eventdev *dlb __rte_unused)
{
	int i;

	/* Addresses will be initialized at port create */
	for (i = 0; i < DLB_MAX_NUM_PORTS; i++) {
		/* First directed ports */

		/* producer port */
		dlb_port[i][DLB_DIR].pp_addr = NULL;

		/* popcount */
		dlb_port[i][DLB_DIR].ldb_popcount = NULL;
		dlb_port[i][DLB_DIR].dir_popcount = NULL;

		/* consumer queue */
		dlb_port[i][DLB_DIR].cq_base = NULL;
		dlb_port[i][DLB_DIR].mmaped = true;

		/* Now load balanced ports */

		/* producer port */
		dlb_port[i][DLB_LDB].pp_addr = NULL;

		/* popcount */
		dlb_port[i][DLB_LDB].ldb_popcount = NULL;
		dlb_port[i][DLB_LDB].dir_popcount = NULL;

		/* consumer queue */
		dlb_port[i][DLB_LDB].cq_base = NULL;
		dlb_port[i][DLB_LDB].mmaped = true;
	}
}

static int
dlb_pf_open(struct dlb_hw_dev *handle, const char *name)
{
	RTE_SET_USED(handle);
	RTE_SET_USED(name);

	return 0;
}

static void
dlb_pf_domain_close(struct dlb_eventdev *dlb)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)dlb->qm_instance.pf_dev;
	int ret;

	ret = dlb_reset_domain(&dlb_dev->hw, dlb->qm_instance.domain_id);
	if (ret)
		DLB_LOG_ERR("dlb_pf_reset_domain err %d", ret);
}

static int
dlb_pf_get_device_version(struct dlb_hw_dev *handle,
			  uint8_t *revision)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;

	*revision = dlb_dev->revision;

	return 0;
}

static int
dlb_pf_get_num_resources(struct dlb_hw_dev *handle,
			 struct dlb_get_num_resources_args *rsrcs)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;

	dlb_hw_get_num_resources(&dlb_dev->hw, rsrcs);

	return 0;
}

static int
dlb_pf_sched_domain_create(struct dlb_hw_dev *handle,
			   struct dlb_create_sched_domain_args *arg)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;

	DLB_INFO(dev->dlb_device, "Entering %s()\n", __func__);

	if (dlb_dev->domain_reset_failed) {
		response.status = DLB_ST_DOMAIN_RESET_FAILED;
		ret = -EINVAL;
		goto done;
	}

	ret = dlb_hw_create_sched_domain(&dlb_dev->hw, arg, &response);
	if (ret)
		goto done;

done:

	*(struct dlb_cmd_response *)arg->response = response;

	DLB_INFO(dev->dlb_device, "Exiting %s() with ret=%d\n", __func__, ret);

	return ret;
}

static int
dlb_pf_ldb_credit_pool_create(struct dlb_hw_dev *handle,
			      struct dlb_create_ldb_pool_args *cfg)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;

	DLB_INFO(dev->dlb_device, "Entering %s()\n", __func__);

	ret = dlb_hw_create_ldb_pool(&dlb_dev->hw,
				     handle->domain_id,
				     cfg,
				     &response);

	*(struct dlb_cmd_response *)cfg->response = response;

	DLB_INFO(dev->dlb_device, "Exiting %s() with ret=%d\n", __func__, ret);

	return ret;
}

static int
dlb_pf_dir_credit_pool_create(struct dlb_hw_dev *handle,
			      struct dlb_create_dir_pool_args *cfg)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;

	DLB_INFO(dev->dlb_device, "Entering %s()\n", __func__);

	ret = dlb_hw_create_dir_pool(&dlb_dev->hw,
				     handle->domain_id,
				     cfg,
				     &response);

	*(struct dlb_cmd_response *)cfg->response = response;

	DLB_INFO(dev->dlb_device, "Exiting %s() with ret=%d\n", __func__, ret);

	return ret;
}

static int
dlb_pf_get_cq_poll_mode(struct dlb_hw_dev *handle,
			enum dlb_cq_poll_modes *mode)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;

	if (dlb_dev->revision >= DLB_REV_B0)
		*mode = DLB_CQ_POLL_MODE_SPARSE;
	else
		*mode = DLB_CQ_POLL_MODE_STD;

	return 0;
}

static int
dlb_pf_ldb_queue_create(struct dlb_hw_dev *handle,
			struct dlb_create_ldb_queue_args *cfg)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;

	DLB_INFO(dev->dlb_device, "Entering %s()\n", __func__);

	ret = dlb_hw_create_ldb_queue(&dlb_dev->hw,
				      handle->domain_id,
				      cfg,
				      &response);

	*(struct dlb_cmd_response *)cfg->response = response;

	DLB_INFO(dev->dlb_device, "Exiting %s() with ret=%d\n", __func__, ret);

	return ret;
}

static int
dlb_pf_dir_queue_create(struct dlb_hw_dev *handle,
			struct dlb_create_dir_queue_args *cfg)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;

	DLB_INFO(dev->dlb_device, "Entering %s()\n", __func__);

	ret = dlb_hw_create_dir_queue(&dlb_dev->hw,
				      handle->domain_id,
				      cfg,
				      &response);

	*(struct dlb_cmd_response *)cfg->response = response;

	DLB_INFO(dev->dlb_device, "Exiting %s() with ret=%d\n", __func__, ret);

	return ret;
}

static void *
dlb_alloc_coherent_aligned(const struct rte_memzone **mz, rte_iova_t *phys,
			   size_t size, int align)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	uint32_t core_id = rte_lcore_id();
	unsigned int socket_id;

	snprintf(mz_name, sizeof(mz_name) - 1, "event_dlb_port_mem_%lx",
		 (unsigned long)rte_get_timer_cycles());
	if (core_id == (unsigned int)LCORE_ID_ANY)
		core_id = rte_get_main_lcore();
	socket_id = rte_lcore_to_socket_id(core_id);
	*mz = rte_memzone_reserve_aligned(mz_name, size, socket_id,
					 RTE_MEMZONE_IOVA_CONTIG, align);
	if (*mz == NULL) {
		DLB_LOG_ERR("Unable to allocate DMA memory of size %zu bytes\n",
			    size);
		*phys = 0;
		return NULL;
	}
	*phys = (*mz)->iova;
	return (*mz)->addr;
}

static int
dlb_pf_ldb_port_create(struct dlb_hw_dev *handle,
		       struct dlb_create_ldb_port_args *cfg,
		       enum dlb_cq_poll_modes poll_mode)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;
	uint8_t *port_base;
	const struct rte_memzone *mz;
	int alloc_sz, qe_sz, cq_alloc_depth;
	rte_iova_t pp_dma_base;
	rte_iova_t pc_dma_base;
	rte_iova_t cq_dma_base;
	int is_dir = false;

	DLB_INFO(dev->dlb_device, "Entering %s()\n", __func__);

	if (poll_mode == DLB_CQ_POLL_MODE_STD)
		qe_sz = sizeof(struct dlb_dequeue_qe);
	else
		qe_sz = RTE_CACHE_LINE_SIZE;

	/* The hardware always uses a CQ depth of at least
	 * DLB_MIN_HARDWARE_CQ_DEPTH, even though from the user
	 * perspective we support a depth as low as 1 for LDB ports.
	 */
	cq_alloc_depth = RTE_MAX(cfg->cq_depth, DLB_MIN_HARDWARE_CQ_DEPTH);

	/* Calculate the port memory required, including two cache lines for
	 * credit pop counts. Round up to the nearest cache line.
	 */
	alloc_sz = 2 * RTE_CACHE_LINE_SIZE + cq_alloc_depth * qe_sz;
	alloc_sz = RTE_CACHE_LINE_ROUNDUP(alloc_sz);

	port_base = dlb_alloc_coherent_aligned(&mz, &pc_dma_base,
					       alloc_sz, PAGE_SIZE);
	if (port_base == NULL)
		return -ENOMEM;

	/* Lock the page in memory */
	ret = rte_mem_lock_page(port_base);
	if (ret < 0) {
		DLB_LOG_ERR("dlb pf pmd could not lock page for device i/o\n");
		goto create_port_err;
	}

	memset(port_base, 0, alloc_sz);
	cq_dma_base = (uintptr_t)(pc_dma_base + (2 * RTE_CACHE_LINE_SIZE));

	ret = dlb_hw_create_ldb_port(&dlb_dev->hw,
				     handle->domain_id,
				     cfg,
				     pc_dma_base,
				     cq_dma_base,
				     &response);
	if (ret)
		goto create_port_err;

	pp_dma_base = (uintptr_t)dlb_dev->hw.func_kva + PP_BASE(is_dir);
	dlb_port[response.id][DLB_LDB].pp_addr =
		(void *)(uintptr_t)(pp_dma_base + (PAGE_SIZE * response.id));

	dlb_port[response.id][DLB_LDB].cq_base =
		(void *)(uintptr_t)(port_base + (2 * RTE_CACHE_LINE_SIZE));

	dlb_port[response.id][DLB_LDB].ldb_popcount =
		(void *)(uintptr_t)port_base;
	dlb_port[response.id][DLB_LDB].dir_popcount = (void *)(uintptr_t)
		(port_base + RTE_CACHE_LINE_SIZE);
	dlb_port[response.id][DLB_LDB].mz = mz;

	*(struct dlb_cmd_response *)cfg->response = response;

	DLB_INFO(dev->dlb_device, "Exiting %s() with ret=%d\n", __func__, ret);
	return 0;

create_port_err:

	rte_memzone_free(mz);

	return ret;
}

static int
dlb_pf_dir_port_create(struct dlb_hw_dev *handle,
		       struct dlb_create_dir_port_args *cfg,
		       enum dlb_cq_poll_modes poll_mode)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;
	uint8_t *port_base;
	const struct rte_memzone *mz;
	int alloc_sz, qe_sz;
	rte_iova_t pp_dma_base;
	rte_iova_t pc_dma_base;
	rte_iova_t cq_dma_base;
	int is_dir = true;

	DLB_INFO(dev->dlb_device, "Entering %s()\n", __func__);

	if (poll_mode == DLB_CQ_POLL_MODE_STD)
		qe_sz = sizeof(struct dlb_dequeue_qe);
	else
		qe_sz = RTE_CACHE_LINE_SIZE;

	/* Calculate the port memory required, including two cache lines for
	 * credit pop counts. Round up to the nearest cache line.
	 */
	alloc_sz = 2 * RTE_CACHE_LINE_SIZE + cfg->cq_depth * qe_sz;
	alloc_sz = RTE_CACHE_LINE_ROUNDUP(alloc_sz);

	port_base = dlb_alloc_coherent_aligned(&mz, &pc_dma_base,
					       alloc_sz, PAGE_SIZE);
	if (port_base == NULL)
		return -ENOMEM;

	/* Lock the page in memory */
	ret = rte_mem_lock_page(port_base);
	if (ret < 0) {
		DLB_LOG_ERR("dlb pf pmd could not lock page for device i/o\n");
		goto create_port_err;
	}

	memset(port_base, 0, alloc_sz);
	cq_dma_base = (uintptr_t)(pc_dma_base + (2 * RTE_CACHE_LINE_SIZE));

	ret = dlb_hw_create_dir_port(&dlb_dev->hw,
				     handle->domain_id,
				     cfg,
				     pc_dma_base,
				     cq_dma_base,
				     &response);
	if (ret)
		goto create_port_err;

	pp_dma_base = (uintptr_t)dlb_dev->hw.func_kva + PP_BASE(is_dir);
	dlb_port[response.id][DLB_DIR].pp_addr =
		(void *)(uintptr_t)(pp_dma_base + (PAGE_SIZE * response.id));

	dlb_port[response.id][DLB_DIR].cq_base =
		(void *)(uintptr_t)(port_base + (2 * RTE_CACHE_LINE_SIZE));

	dlb_port[response.id][DLB_DIR].ldb_popcount =
		(void *)(uintptr_t)port_base;
	dlb_port[response.id][DLB_DIR].dir_popcount = (void *)(uintptr_t)
		(port_base + RTE_CACHE_LINE_SIZE);
	dlb_port[response.id][DLB_DIR].mz = mz;

	*(struct dlb_cmd_response *)cfg->response = response;

	DLB_INFO(dev->dlb_device, "Exiting %s() with ret=%d\n", __func__, ret);
	return 0;

create_port_err:

	rte_memzone_free(mz);

	return ret;
}

static int
dlb_pf_get_sn_allocation(struct dlb_hw_dev *handle,
			 struct dlb_get_sn_allocation_args *args)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;

	ret = dlb_get_group_sequence_numbers(&dlb_dev->hw, args->group);

	response.id = ret;
	response.status = 0;

	*(struct dlb_cmd_response *)args->response = response;

	return ret;
}

static int
dlb_pf_set_sn_allocation(struct dlb_hw_dev *handle,
			 struct dlb_set_sn_allocation_args *args)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;

	ret = dlb_set_group_sequence_numbers(&dlb_dev->hw, args->group,
					     args->num);

	response.status = 0;

	*(struct dlb_cmd_response *)args->response = response;

	return ret;
}

static int
dlb_pf_get_sn_occupancy(struct dlb_hw_dev *handle,
			struct dlb_get_sn_occupancy_args *args)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;

	ret = dlb_get_group_sequence_number_occupancy(&dlb_dev->hw,
						      args->group);

	response.id = ret;
	response.status = 0;

	*(struct dlb_cmd_response *)args->response = response;

	return ret;
}

static int
dlb_pf_sched_domain_start(struct dlb_hw_dev *handle,
			  struct dlb_start_domain_args *cfg)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;

	DLB_INFO(dev->dlb_device, "Entering %s()\n", __func__);

	ret = dlb_hw_start_domain(&dlb_dev->hw,
				  handle->domain_id,
				  cfg,
				  &response);

	*(struct dlb_cmd_response *)cfg->response = response;

	DLB_INFO(dev->dlb_device, "Exiting %s() with ret=%d\n", __func__, ret);

	return ret;
}

static int
dlb_pf_pending_port_unmaps(struct dlb_hw_dev *handle,
			   struct dlb_pending_port_unmaps_args *args)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;

	DLB_INFO(dev->dlb_device, "Entering %s()\n", __func__);

	ret = dlb_hw_pending_port_unmaps(&dlb_dev->hw,
					 handle->domain_id,
					 args,
					 &response);

	*(struct dlb_cmd_response *)args->response = response;

	DLB_INFO(dev->dlb_device, "Exiting %s() with ret=%d\n", __func__, ret);

	return ret;
}

static int
dlb_pf_map_qid(struct dlb_hw_dev *handle,
	       struct dlb_map_qid_args *cfg)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;

	DLB_INFO(dev->dlb_device, "Entering %s()\n", __func__);

	ret = dlb_hw_map_qid(&dlb_dev->hw,
			     handle->domain_id,
			     cfg,
			     &response);

	*(struct dlb_cmd_response *)cfg->response = response;

	DLB_INFO(dev->dlb_device, "Exiting %s() with ret=%d\n", __func__, ret);

	return ret;
}

static int
dlb_pf_unmap_qid(struct dlb_hw_dev *handle,
		 struct dlb_unmap_qid_args *cfg)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;

	DLB_INFO(dev->dlb_device, "Entering %s()\n", __func__);

	ret = dlb_hw_unmap_qid(&dlb_dev->hw,
			       handle->domain_id,
			       cfg,
			       &response);

	*(struct dlb_cmd_response *)cfg->response = response;

	DLB_INFO(dev->dlb_device, "Exiting %s() with ret=%d\n", __func__, ret);

	return ret;
}

static int
dlb_pf_get_ldb_queue_depth(struct dlb_hw_dev *handle,
			   struct dlb_get_ldb_queue_depth_args *args)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret;

	DLB_INFO(dev->dlb_device, "Entering %s()\n", __func__);

	ret = dlb_hw_get_ldb_queue_depth(&dlb_dev->hw,
					 handle->domain_id,
					 args,
					 &response);

	*(struct dlb_cmd_response *)args->response = response;

	DLB_INFO(dev->dlb_device, "Exiting %s() with ret=%d\n", __func__, ret);

	return ret;
}

static int
dlb_pf_get_dir_queue_depth(struct dlb_hw_dev *handle,
			   struct dlb_get_dir_queue_depth_args *args)
{
	struct dlb_dev *dlb_dev = (struct dlb_dev *)handle->pf_dev;
	struct dlb_cmd_response response = {0};
	int ret = 0;

	DLB_INFO(dev->dlb_device, "Entering %s()\n", __func__);

	ret = dlb_hw_get_dir_queue_depth(&dlb_dev->hw,
					 handle->domain_id,
					 args,
					 &response);

	*(struct dlb_cmd_response *)args->response = response;

	DLB_INFO(dev->dlb_device, "Exiting %s() with ret=%d\n", __func__, ret);

	return ret;
}

static void
dlb_pf_iface_fn_ptrs_init(void)
{
	dlb_iface_low_level_io_init = dlb_pf_low_level_io_init;
	dlb_iface_open = dlb_pf_open;
	dlb_iface_domain_close = dlb_pf_domain_close;
	dlb_iface_get_device_version = dlb_pf_get_device_version;
	dlb_iface_get_num_resources = dlb_pf_get_num_resources;
	dlb_iface_sched_domain_create = dlb_pf_sched_domain_create;
	dlb_iface_ldb_credit_pool_create = dlb_pf_ldb_credit_pool_create;
	dlb_iface_dir_credit_pool_create = dlb_pf_dir_credit_pool_create;
	dlb_iface_ldb_queue_create = dlb_pf_ldb_queue_create;
	dlb_iface_dir_queue_create = dlb_pf_dir_queue_create;
	dlb_iface_ldb_port_create = dlb_pf_ldb_port_create;
	dlb_iface_dir_port_create = dlb_pf_dir_port_create;
	dlb_iface_map_qid = dlb_pf_map_qid;
	dlb_iface_unmap_qid = dlb_pf_unmap_qid;
	dlb_iface_sched_domain_start = dlb_pf_sched_domain_start;
	dlb_iface_pending_port_unmaps = dlb_pf_pending_port_unmaps;
	dlb_iface_get_ldb_queue_depth = dlb_pf_get_ldb_queue_depth;
	dlb_iface_get_dir_queue_depth = dlb_pf_get_dir_queue_depth;
	dlb_iface_get_cq_poll_mode = dlb_pf_get_cq_poll_mode;
	dlb_iface_get_sn_allocation = dlb_pf_get_sn_allocation;
	dlb_iface_set_sn_allocation = dlb_pf_set_sn_allocation;
	dlb_iface_get_sn_occupancy = dlb_pf_get_sn_occupancy;

}

/* PCI DEV HOOKS */
static int
dlb_eventdev_pci_init(struct rte_eventdev *eventdev)
{
	int ret = 0;
	struct rte_pci_device *pci_dev;
	struct dlb_devargs dlb_args = {
		.socket_id = rte_socket_id(),
		.max_num_events = DLB_MAX_NUM_LDB_CREDITS,
		.num_dir_credits_override = -1,
		.defer_sched = 0,
		.num_atm_inflights = DLB_NUM_ATOMIC_INFLIGHTS_PER_QUEUE,
	};
	struct dlb_eventdev *dlb;

	DLB_LOG_DBG("Enter with dev_id=%d socket_id=%d",
		    eventdev->data->dev_id, eventdev->data->socket_id);

	dlb_entry_points_init(eventdev);

	dlb_pf_iface_fn_ptrs_init();

	pci_dev = RTE_DEV_TO_PCI(eventdev->dev);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		dlb = dlb_pmd_priv(eventdev); /* rte_zmalloc_socket mem */

		/* Probe the DLB PF layer */
		dlb->qm_instance.pf_dev = dlb_probe(pci_dev);

		if (dlb->qm_instance.pf_dev == NULL) {
			DLB_LOG_ERR("DLB PF Probe failed with error %d\n",
				    rte_errno);
			ret = -rte_errno;
			goto dlb_probe_failed;
		}

		/* Were we invoked with runtime parameters? */
		if (pci_dev->device.devargs) {
			ret = dlb_parse_params(pci_dev->device.devargs->args,
					       pci_dev->device.devargs->name,
					       &dlb_args);
			if (ret) {
				DLB_LOG_ERR("PFPMD failed to parse args ret=%d, errno=%d\n",
					    ret, rte_errno);
				goto dlb_probe_failed;
			}
		}

		ret = dlb_primary_eventdev_probe(eventdev,
						 EVDEV_DLB_NAME_PMD_STR,
						 &dlb_args);
	} else {
		ret = dlb_secondary_eventdev_probe(eventdev,
						   EVDEV_DLB_NAME_PMD_STR);
	}
	if (ret)
		goto dlb_probe_failed;

	DLB_LOG_INFO("DLB PF Probe success\n");

	return 0;

dlb_probe_failed:

	DLB_LOG_INFO("DLB PF Probe failed, ret=%d\n", ret);

	return ret;
}

#define EVENTDEV_INTEL_VENDOR_ID 0x8086

static const struct rte_pci_id pci_id_dlb_map[] = {
	{
		RTE_PCI_DEVICE(EVENTDEV_INTEL_VENDOR_ID,
			       DLB_PF_DEV_ID)
	},
	{
		.vendor_id = 0,
	},
};

static int
event_dlb_pci_probe(struct rte_pci_driver *pci_drv,
		    struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_probe_named(pci_drv, pci_dev,
		sizeof(struct dlb_eventdev), dlb_eventdev_pci_init,
		EVDEV_DLB_NAME_PMD_STR);
}

static int
event_dlb_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_remove(pci_dev, NULL);
}

static struct rte_pci_driver pci_eventdev_dlb_pmd = {
	.id_table = pci_id_dlb_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = event_dlb_pci_probe,
	.remove = event_dlb_pci_remove,
};

RTE_PMD_REGISTER_PCI(event_dlb_pf, pci_eventdev_dlb_pmd);
RTE_PMD_REGISTER_PCI_TABLE(event_dlb_pf, pci_id_dlb_map);
