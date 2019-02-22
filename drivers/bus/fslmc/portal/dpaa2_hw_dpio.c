/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016-2018 NXP
 *
 */
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <inttypes.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/epoll.h>
#include<sys/eventfd.h>

#include <rte_mbuf.h>
#include <rte_ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_dev.h>

#include <fslmc_logs.h>
#include <rte_fslmc.h>
#include "dpaa2_hw_pvt.h"
#include "dpaa2_hw_dpio.h"
#include <mc/fsl_dpmng.h>

#define NUM_HOST_CPUS RTE_MAX_LCORE

struct dpaa2_io_portal_t dpaa2_io_portal[RTE_MAX_LCORE];
RTE_DEFINE_PER_LCORE(struct dpaa2_io_portal_t, _dpaa2_io);

struct swp_active_dqs rte_global_active_dqs_list[NUM_MAX_SWP];

TAILQ_HEAD(dpio_dev_list, dpaa2_dpio_dev);
static struct dpio_dev_list dpio_dev_list
	= TAILQ_HEAD_INITIALIZER(dpio_dev_list); /*!< DPIO device list */
static uint32_t io_space_count;

/* Variable to store DPAA2 platform type */
uint32_t dpaa2_svr_family;

/* Physical core id for lcores running on dpaa2. */
/* DPAA2 only support 1 lcore to 1 phy cpu mapping */
static unsigned int dpaa2_cpu[RTE_MAX_LCORE];

/* Variable to store DPAA2 DQRR size */
uint8_t dpaa2_dqrr_size;
/* Variable to store DPAA2 EQCR size */
uint8_t dpaa2_eqcr_size;

/*Stashing Macros default for LS208x*/
static int dpaa2_core_cluster_base = 0x04;
static int dpaa2_cluster_sz = 2;

/* For LS208X platform There are four clusters with following mapping:
 * Cluster 1 (ID = x04) : CPU0, CPU1;
 * Cluster 2 (ID = x05) : CPU2, CPU3;
 * Cluster 3 (ID = x06) : CPU4, CPU5;
 * Cluster 4 (ID = x07) : CPU6, CPU7;
 */
/* For LS108X platform There are two clusters with following mapping:
 * Cluster 1 (ID = x02) : CPU0, CPU1, CPU2, CPU3;
 * Cluster 2 (ID = x03) : CPU4, CPU5, CPU6, CPU7;
 */
/* For LX2160 platform There are four clusters with following mapping:
 * Cluster 1 (ID = x00) : CPU0, CPU1;
 * Cluster 2 (ID = x01) : CPU2, CPU3;
 * Cluster 3 (ID = x02) : CPU4, CPU5;
 * Cluster 4 (ID = x03) : CPU6, CPU7;
 * Cluster 1 (ID = x04) : CPU8, CPU9;
 * Cluster 2 (ID = x05) : CPU10, CP11;
 * Cluster 3 (ID = x06) : CPU12, CPU13;
 * Cluster 4 (ID = x07) : CPU14, CPU15;
 */

static int
dpaa2_core_cluster_sdest(int cpu_id)
{
	int x = cpu_id / dpaa2_cluster_sz;

	return dpaa2_core_cluster_base + x;
}

#ifdef RTE_LIBRTE_PMD_DPAA2_EVENTDEV
static void
dpaa2_affine_dpio_intr_to_respective_core(int32_t dpio_id, int lcoreid)
{
#define STRING_LEN	28
#define COMMAND_LEN	50
	uint32_t cpu_mask = 1;
	int ret;
	size_t len = 0;
	char *temp = NULL, *token = NULL;
	char string[STRING_LEN], command[COMMAND_LEN];
	FILE *file;

	snprintf(string, STRING_LEN, "dpio.%d", dpio_id);
	file = fopen("/proc/interrupts", "r");
	if (!file) {
		DPAA2_BUS_WARN("Failed to open /proc/interrupts file");
		return;
	}
	while (getline(&temp, &len, file) != -1) {
		if ((strstr(temp, string)) != NULL) {
			token = strtok(temp, ":");
			break;
		}
	}

	if (!token) {
		DPAA2_BUS_WARN("Failed to get interrupt id for dpio.%d",
			       dpio_id);
		if (temp)
			free(temp);
		fclose(file);
		return;
	}

	cpu_mask = cpu_mask << dpaa2_cpu[lcoreid];
	snprintf(command, COMMAND_LEN, "echo %X > /proc/irq/%s/smp_affinity",
		 cpu_mask, token);
	ret = system(command);
	if (ret < 0)
		DPAA2_BUS_DEBUG(
			"Failed to affine interrupts on respective core");
	else
		DPAA2_BUS_DEBUG(" %s command is executed", command);

	free(temp);
	fclose(file);
}

static int dpaa2_dpio_intr_init(struct dpaa2_dpio_dev *dpio_dev, int lcoreid)
{
	struct epoll_event epoll_ev;
	int eventfd, dpio_epoll_fd, ret;
	int threshold = 0x3, timeout = 0xFF;

	dpio_epoll_fd = epoll_create(1);
	ret = rte_dpaa2_intr_enable(&dpio_dev->intr_handle, 0);
	if (ret) {
		DPAA2_BUS_ERR("Interrupt registeration failed");
		return -1;
	}

	if (getenv("DPAA2_PORTAL_INTR_THRESHOLD"))
		threshold = atoi(getenv("DPAA2_PORTAL_INTR_THRESHOLD"));

	if (getenv("DPAA2_PORTAL_INTR_TIMEOUT"))
		sscanf(getenv("DPAA2_PORTAL_INTR_TIMEOUT"), "%x", &timeout);

	qbman_swp_interrupt_set_trigger(dpio_dev->sw_portal,
					QBMAN_SWP_INTERRUPT_DQRI);
	qbman_swp_interrupt_clear_status(dpio_dev->sw_portal, 0xffffffff);
	qbman_swp_interrupt_set_inhibit(dpio_dev->sw_portal, 0);
	qbman_swp_dqrr_thrshld_write(dpio_dev->sw_portal, threshold);
	qbman_swp_intr_timeout_write(dpio_dev->sw_portal, timeout);

	eventfd = dpio_dev->intr_handle.fd;
	epoll_ev.events = EPOLLIN | EPOLLPRI | EPOLLET;
	epoll_ev.data.fd = eventfd;

	ret = epoll_ctl(dpio_epoll_fd, EPOLL_CTL_ADD, eventfd, &epoll_ev);
	if (ret < 0) {
		DPAA2_BUS_ERR("epoll_ctl failed");
		return -1;
	}
	dpio_dev->epoll_fd = dpio_epoll_fd;

	dpaa2_affine_dpio_intr_to_respective_core(dpio_dev->hw_id, lcoreid);

	return 0;
}
#endif

static int
dpaa2_configure_stashing(struct dpaa2_dpio_dev *dpio_dev, int lcoreid)
{
	int sdest, ret;
	int cpu_id;

	/* Set the Stashing Destination */
	if (lcoreid < 0) {
		lcoreid = rte_get_master_lcore();
		if (lcoreid < 0) {
			DPAA2_BUS_ERR("Getting CPU Index failed");
			return -1;
		}
	}

	cpu_id = dpaa2_cpu[lcoreid];

	/* Set the STASH Destination depending on Current CPU ID.
	 * Valid values of SDEST are 4,5,6,7. Where,
	 */

	sdest = dpaa2_core_cluster_sdest(cpu_id);
	DPAA2_BUS_DEBUG("Portal= %d  CPU= %u lcore id =%u SDEST= %d",
			dpio_dev->index, cpu_id, lcoreid, sdest);

	ret = dpio_set_stashing_destination(dpio_dev->dpio, CMD_PRI_LOW,
					    dpio_dev->token, sdest);
	if (ret) {
		DPAA2_BUS_ERR("%d ERROR in SDEST",  ret);
		return -1;
	}

#ifdef RTE_LIBRTE_PMD_DPAA2_EVENTDEV
	if (dpaa2_dpio_intr_init(dpio_dev, lcoreid)) {
		DPAA2_BUS_ERR("Interrupt registration failed for dpio");
		return -1;
	}
#endif

	return 0;
}

static struct dpaa2_dpio_dev *dpaa2_get_qbman_swp(int lcoreid)
{
	struct dpaa2_dpio_dev *dpio_dev = NULL;
	int ret;

	/* Get DPIO dev handle from list using index */
	TAILQ_FOREACH(dpio_dev, &dpio_dev_list, next) {
		if (dpio_dev && rte_atomic16_test_and_set(&dpio_dev->ref_count))
			break;
	}
	if (!dpio_dev)
		return NULL;

	DPAA2_BUS_DEBUG("New Portal %p (%d) affined thread - %lu",
			dpio_dev, dpio_dev->index, syscall(SYS_gettid));

	ret = dpaa2_configure_stashing(dpio_dev, lcoreid);
	if (ret)
		DPAA2_BUS_ERR("dpaa2_configure_stashing failed");

	return dpio_dev;
}

int
dpaa2_affine_qbman_swp(void)
{
	unsigned int lcore_id = rte_lcore_id();
	uint64_t tid = syscall(SYS_gettid);

	if (lcore_id == LCORE_ID_ANY)
		lcore_id = rte_get_master_lcore();
	/* if the core id is not supported */
	else if (lcore_id >= RTE_MAX_LCORE)
		return -1;

	if (dpaa2_io_portal[lcore_id].dpio_dev) {
		DPAA2_BUS_DP_INFO("DPAA Portal=%p (%d) is being shared"
			    " between thread %" PRIu64 " and current "
			    "%" PRIu64 "\n",
			    dpaa2_io_portal[lcore_id].dpio_dev,
			    dpaa2_io_portal[lcore_id].dpio_dev->index,
			    dpaa2_io_portal[lcore_id].net_tid,
			    tid);
		RTE_PER_LCORE(_dpaa2_io).dpio_dev
			= dpaa2_io_portal[lcore_id].dpio_dev;
		rte_atomic16_inc(&dpaa2_io_portal
				 [lcore_id].dpio_dev->ref_count);
		dpaa2_io_portal[lcore_id].net_tid = tid;

		DPAA2_BUS_DP_DEBUG("Old Portal=%p (%d) affined thread - "
				   "%" PRIu64 "\n",
			    dpaa2_io_portal[lcore_id].dpio_dev,
			    dpaa2_io_portal[lcore_id].dpio_dev->index,
			    tid);
		return 0;
	}

	/* Populate the dpaa2_io_portal structure */
	dpaa2_io_portal[lcore_id].dpio_dev = dpaa2_get_qbman_swp(lcore_id);

	if (dpaa2_io_portal[lcore_id].dpio_dev) {
		RTE_PER_LCORE(_dpaa2_io).dpio_dev
			= dpaa2_io_portal[lcore_id].dpio_dev;
		dpaa2_io_portal[lcore_id].net_tid = tid;

		return 0;
	} else {
		return -1;
	}
}

int
dpaa2_affine_qbman_ethrx_swp(void)
{
	unsigned int lcore_id = rte_lcore_id();
	uint64_t tid = syscall(SYS_gettid);

	if (lcore_id == LCORE_ID_ANY)
		lcore_id = rte_get_master_lcore();
	/* if the core id is not supported */
	else if (lcore_id >= RTE_MAX_LCORE)
		return -1;

	if (dpaa2_io_portal[lcore_id].ethrx_dpio_dev) {
		DPAA2_BUS_DP_INFO(
			"DPAA Portal=%p (%d) is being shared between thread"
			" %" PRIu64 " and current %" PRIu64 "\n",
			dpaa2_io_portal[lcore_id].ethrx_dpio_dev,
			dpaa2_io_portal[lcore_id].ethrx_dpio_dev->index,
			dpaa2_io_portal[lcore_id].sec_tid,
			tid);
		RTE_PER_LCORE(_dpaa2_io).ethrx_dpio_dev
			= dpaa2_io_portal[lcore_id].ethrx_dpio_dev;
		rte_atomic16_inc(&dpaa2_io_portal
				 [lcore_id].ethrx_dpio_dev->ref_count);
		dpaa2_io_portal[lcore_id].sec_tid = tid;

		DPAA2_BUS_DP_DEBUG(
			"Old Portal=%p (%d) affined thread"
			" - %" PRIu64 "\n",
			dpaa2_io_portal[lcore_id].ethrx_dpio_dev,
			dpaa2_io_portal[lcore_id].ethrx_dpio_dev->index,
			tid);
		return 0;
	}

	/* Populate the dpaa2_io_portal structure */
	dpaa2_io_portal[lcore_id].ethrx_dpio_dev =
		dpaa2_get_qbman_swp(lcore_id);

	if (dpaa2_io_portal[lcore_id].ethrx_dpio_dev) {
		RTE_PER_LCORE(_dpaa2_io).ethrx_dpio_dev
			= dpaa2_io_portal[lcore_id].ethrx_dpio_dev;
		dpaa2_io_portal[lcore_id].sec_tid = tid;
		return 0;
	} else {
		return -1;
	}
}

/*
 * This checks for not supported lcore mappings as well as get the physical
 * cpuid for the lcore.
 * one lcore can only map to 1 cpu i.e. 1@10-14 not supported.
 * one cpu can be mapped to more than one lcores.
 */
static int
dpaa2_check_lcore_cpuset(void)
{
	unsigned int lcore_id, i;
	int ret = 0;

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++)
		dpaa2_cpu[lcore_id] = 0xffffffff;

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		for (i = 0; i < RTE_MAX_LCORE; i++) {
			if (CPU_ISSET(i, &lcore_config[lcore_id].cpuset)) {
				RTE_LOG(DEBUG, EAL, "lcore id = %u cpu=%u\n",
					lcore_id, i);
				if (dpaa2_cpu[lcore_id] != 0xffffffff) {
					DPAA2_BUS_ERR(
				    "ERR:lcore map to multi-cpu not supported");
					ret = -1;
				} else  {
					dpaa2_cpu[lcore_id] = i;
				}
			}
		}
	}
	return ret;
}

static int
dpaa2_create_dpio_device(int vdev_fd,
			 struct vfio_device_info *obj_info,
			 int object_id)
{
	struct dpaa2_dpio_dev *dpio_dev = NULL;
	struct vfio_region_info reg_info = { .argsz = sizeof(reg_info)};
	struct qbman_swp_desc p_des;
	struct dpio_attr attr;
	static int check_lcore_cpuset;

	if (obj_info->num_regions < NUM_DPIO_REGIONS) {
		DPAA2_BUS_ERR("Not sufficient number of DPIO regions");
		return -1;
	}

	dpio_dev = rte_zmalloc(NULL, sizeof(struct dpaa2_dpio_dev),
			      RTE_CACHE_LINE_SIZE);
	if (!dpio_dev) {
		DPAA2_BUS_ERR("Memory allocation failed for DPIO Device");
		return -1;
	}

	dpio_dev->dpio = NULL;
	dpio_dev->hw_id = object_id;
	rte_atomic16_init(&dpio_dev->ref_count);
	/* Using single portal  for all devices */
	dpio_dev->mc_portal = rte_mcp_ptr_list[MC_PORTAL_INDEX];

	if (!check_lcore_cpuset) {
		check_lcore_cpuset = 1;

		if (dpaa2_check_lcore_cpuset() < 0)
			goto err;
	}

	dpio_dev->dpio = rte_zmalloc(NULL, sizeof(struct fsl_mc_io),
				     RTE_CACHE_LINE_SIZE);
	if (!dpio_dev->dpio) {
		DPAA2_BUS_ERR("Memory allocation failure");
		goto err;
	}

	dpio_dev->dpio->regs = dpio_dev->mc_portal;
	if (dpio_open(dpio_dev->dpio, CMD_PRI_LOW, dpio_dev->hw_id,
		      &dpio_dev->token)) {
		DPAA2_BUS_ERR("Failed to allocate IO space");
		goto err;
	}

	if (dpio_reset(dpio_dev->dpio, CMD_PRI_LOW, dpio_dev->token)) {
		DPAA2_BUS_ERR("Failed to reset dpio");
		goto err;
	}

	if (dpio_enable(dpio_dev->dpio, CMD_PRI_LOW, dpio_dev->token)) {
		DPAA2_BUS_ERR("Failed to Enable dpio");
		goto err;
	}

	if (dpio_get_attributes(dpio_dev->dpio, CMD_PRI_LOW,
				dpio_dev->token, &attr)) {
		DPAA2_BUS_ERR("DPIO Get attribute failed");
		goto err;
	}

	/* find the SoC type for the first time */
	if (!dpaa2_svr_family) {
		struct mc_soc_version mc_plat_info = {0};

		if (mc_get_soc_version(dpio_dev->dpio,
				       CMD_PRI_LOW, &mc_plat_info)) {
			DPAA2_BUS_ERR("Unable to get SoC version information");
		} else if ((mc_plat_info.svr & 0xffff0000) == SVR_LS1080A) {
			dpaa2_core_cluster_base = 0x02;
			dpaa2_cluster_sz = 4;
			DPAA2_BUS_DEBUG("LS108x (A53) Platform Detected");
		} else if ((mc_plat_info.svr & 0xffff0000) == SVR_LX2160A) {
			dpaa2_core_cluster_base = 0x00;
			dpaa2_cluster_sz = 2;
			DPAA2_BUS_DEBUG("LX2160 Platform Detected");
		}
		dpaa2_svr_family = (mc_plat_info.svr & 0xffff0000);

		if (dpaa2_svr_family == SVR_LX2160A) {
			dpaa2_dqrr_size = DPAA2_LX2_DQRR_RING_SIZE;
			dpaa2_eqcr_size = DPAA2_LX2_EQCR_RING_SIZE;
		} else {
			dpaa2_dqrr_size = DPAA2_DQRR_RING_SIZE;
			dpaa2_eqcr_size = DPAA2_EQCR_RING_SIZE;
		}
	}

	if (dpaa2_svr_family == SVR_LX2160A)
		reg_info.index = DPAA2_SWP_CENA_MEM_REGION;
	else
		reg_info.index = DPAA2_SWP_CENA_REGION;

	if (ioctl(vdev_fd, VFIO_DEVICE_GET_REGION_INFO, &reg_info)) {
		DPAA2_BUS_ERR("vfio: error getting region info");
		goto err;
	}

	dpio_dev->ce_size = reg_info.size;
	dpio_dev->qbman_portal_ce_paddr = (size_t)mmap(NULL, reg_info.size,
				PROT_WRITE | PROT_READ, MAP_SHARED,
				vdev_fd, reg_info.offset);

	reg_info.index = DPAA2_SWP_CINH_REGION;
	if (ioctl(vdev_fd, VFIO_DEVICE_GET_REGION_INFO, &reg_info)) {
		DPAA2_BUS_ERR("vfio: error getting region info");
		goto err;
	}

	dpio_dev->ci_size = reg_info.size;
	dpio_dev->qbman_portal_ci_paddr = (size_t)mmap(NULL, reg_info.size,
				PROT_WRITE | PROT_READ, MAP_SHARED,
				vdev_fd, reg_info.offset);

	/* Configure & setup SW portal */
	p_des.block = NULL;
	p_des.idx = attr.qbman_portal_id;
	p_des.cena_bar = (void *)(dpio_dev->qbman_portal_ce_paddr);
	p_des.cinh_bar = (void *)(dpio_dev->qbman_portal_ci_paddr);
	p_des.irq = -1;
	p_des.qman_version = attr.qbman_version;
	p_des.eqcr_mode = qman_eqcr_vb_ring;
	p_des.cena_access_mode = qman_cena_fastest_access;

	dpio_dev->sw_portal = qbman_swp_init(&p_des);
	if (dpio_dev->sw_portal == NULL) {
		DPAA2_BUS_ERR("QBMan SW Portal Init failed");
		goto err;
	}

	io_space_count++;
	dpio_dev->index = io_space_count;

	if (rte_dpaa2_vfio_setup_intr(&dpio_dev->intr_handle, vdev_fd, 1)) {
		DPAA2_BUS_ERR("Fail to setup interrupt for %d",
			      dpio_dev->hw_id);
		goto err;
	}

	dpio_dev->eqresp = rte_zmalloc(NULL, MAX_EQ_RESP_ENTRIES *
				     (sizeof(struct qbman_result) +
				     sizeof(struct eqresp_metadata)),
				     RTE_CACHE_LINE_SIZE);
	if (!dpio_dev->eqresp) {
		DPAA2_BUS_ERR("Memory allocation failed for eqresp");
		goto err;
	}
	dpio_dev->eqresp_meta = (struct eqresp_metadata *)(dpio_dev->eqresp +
				MAX_EQ_RESP_ENTRIES);


	TAILQ_INSERT_TAIL(&dpio_dev_list, dpio_dev, next);

	return 0;

err:
	if (dpio_dev->dpio) {
		dpio_disable(dpio_dev->dpio, CMD_PRI_LOW, dpio_dev->token);
		dpio_close(dpio_dev->dpio, CMD_PRI_LOW,  dpio_dev->token);
		rte_free(dpio_dev->dpio);
	}

	rte_free(dpio_dev);

	/* For each element in the list, cleanup */
	TAILQ_FOREACH(dpio_dev, &dpio_dev_list, next) {
		if (dpio_dev->dpio) {
			dpio_disable(dpio_dev->dpio, CMD_PRI_LOW,
				dpio_dev->token);
			dpio_close(dpio_dev->dpio, CMD_PRI_LOW,
				dpio_dev->token);
			rte_free(dpio_dev->dpio);
		}
		rte_free(dpio_dev);
	}

	/* Preventing re-use of the list with old entries */
	TAILQ_INIT(&dpio_dev_list);

	return -1;
}

void
dpaa2_free_dq_storage(struct queue_storage_info_t *q_storage)
{
	int i = 0;

	for (i = 0; i < NUM_DQS_PER_QUEUE; i++) {
		if (q_storage->dq_storage[i])
			rte_free(q_storage->dq_storage[i]);
	}
}

int
dpaa2_alloc_dq_storage(struct queue_storage_info_t *q_storage)
{
	int i = 0;

	for (i = 0; i < NUM_DQS_PER_QUEUE; i++) {
		q_storage->dq_storage[i] = rte_malloc(NULL,
			dpaa2_dqrr_size * sizeof(struct qbman_result),
			RTE_CACHE_LINE_SIZE);
		if (!q_storage->dq_storage[i])
			goto fail;
	}
	return 0;
fail:
	while (--i >= 0)
		rte_free(q_storage->dq_storage[i]);

	return -1;
}

uint32_t
dpaa2_free_eq_descriptors(void)
{
	struct dpaa2_dpio_dev *dpio_dev = DPAA2_PER_LCORE_DPIO;
	struct qbman_result *eqresp;
	struct eqresp_metadata *eqresp_meta;
	struct dpaa2_queue *txq;

	while (dpio_dev->eqresp_ci != dpio_dev->eqresp_pi) {
		eqresp = &dpio_dev->eqresp[dpio_dev->eqresp_ci];
		eqresp_meta = &dpio_dev->eqresp_meta[dpio_dev->eqresp_ci];

		if (!qbman_result_eqresp_rspid(eqresp))
			break;

		if (qbman_result_eqresp_rc(eqresp)) {
			txq = eqresp_meta->dpaa2_q;
			txq->cb_eqresp_free(dpio_dev->eqresp_ci);
		}
		qbman_result_eqresp_set_rspid(eqresp, 0);

		dpio_dev->eqresp_ci + 1 < MAX_EQ_RESP_ENTRIES ?
			dpio_dev->eqresp_ci++ : (dpio_dev->eqresp_ci = 0);
	}

	/* Return 1 less entry so that PI and CI are never same in a
	 * case there all the EQ responses are in use.
	 */
	if (dpio_dev->eqresp_ci > dpio_dev->eqresp_pi)
		return dpio_dev->eqresp_ci - dpio_dev->eqresp_pi - 1;
	else
		return dpio_dev->eqresp_ci - dpio_dev->eqresp_pi +
			MAX_EQ_RESP_ENTRIES - 1;
}

static struct rte_dpaa2_object rte_dpaa2_dpio_obj = {
	.dev_type = DPAA2_IO,
	.create = dpaa2_create_dpio_device,
};

RTE_PMD_REGISTER_DPAA2_OBJECT(dpio, rte_dpaa2_dpio_obj);
