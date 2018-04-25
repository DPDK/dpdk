/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016 NXP
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
static void dpaa2_affine_dpio_intr_to_respective_core(int32_t dpio_id)
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

	cpu_mask = cpu_mask << rte_lcore_id();
	snprintf(command, COMMAND_LEN, "echo %X > /proc/irq/%s/smp_affinity",
		 cpu_mask, token);
	ret = system(command);
	if (ret < 0)
		DPAA2_BUS_WARN(
			"Failed to affine interrupts on respective core");
	else
		DPAA2_BUS_DEBUG(" %s command is executed", command);

	free(temp);
	fclose(file);
}

static int dpaa2_dpio_intr_init(struct dpaa2_dpio_dev *dpio_dev)
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

	dpaa2_affine_dpio_intr_to_respective_core(dpio_dev->hw_id);

	return 0;
}
#endif

static int
configure_dpio_qbman_swp(struct dpaa2_dpio_dev *dpio_dev)
{
	struct qbman_swp_desc p_des;
	struct dpio_attr attr;

	dpio_dev->dpio = malloc(sizeof(struct fsl_mc_io));
	if (!dpio_dev->dpio) {
		DPAA2_BUS_ERR("Memory allocation failure");
		return -1;
	}

	dpio_dev->dpio->regs = dpio_dev->mc_portal;
	if (dpio_open(dpio_dev->dpio, CMD_PRI_LOW, dpio_dev->hw_id,
		      &dpio_dev->token)) {
		DPAA2_BUS_ERR("Failed to allocate IO space");
		free(dpio_dev->dpio);
		return -1;
	}

	if (dpio_reset(dpio_dev->dpio, CMD_PRI_LOW, dpio_dev->token)) {
		DPAA2_BUS_ERR("Failed to reset dpio");
		dpio_close(dpio_dev->dpio, CMD_PRI_LOW, dpio_dev->token);
		free(dpio_dev->dpio);
		return -1;
	}

	if (dpio_enable(dpio_dev->dpio, CMD_PRI_LOW, dpio_dev->token)) {
		DPAA2_BUS_ERR("Failed to Enable dpio");
		dpio_close(dpio_dev->dpio, CMD_PRI_LOW, dpio_dev->token);
		free(dpio_dev->dpio);
		return -1;
	}

	if (dpio_get_attributes(dpio_dev->dpio, CMD_PRI_LOW,
				dpio_dev->token, &attr)) {
		DPAA2_BUS_ERR("DPIO Get attribute failed");
		dpio_disable(dpio_dev->dpio, CMD_PRI_LOW, dpio_dev->token);
		dpio_close(dpio_dev->dpio, CMD_PRI_LOW,  dpio_dev->token);
		free(dpio_dev->dpio);
		return -1;
	}

	/* Configure & setup SW portal */
	p_des.block = NULL;
	p_des.idx = attr.qbman_portal_id;
	p_des.cena_bar = (void *)(dpio_dev->qbman_portal_ce_paddr);
	p_des.cinh_bar = (void *)(dpio_dev->qbman_portal_ci_paddr);
	p_des.irq = -1;
	p_des.qman_version = attr.qbman_version;

	dpio_dev->sw_portal = qbman_swp_init(&p_des);
	if (dpio_dev->sw_portal == NULL) {
		DPAA2_BUS_ERR("QBMan SW Portal Init failed");
		dpio_close(dpio_dev->dpio, CMD_PRI_LOW, dpio_dev->token);
		free(dpio_dev->dpio);
		return -1;
	}

	return 0;
}

static int
dpaa2_configure_stashing(struct dpaa2_dpio_dev *dpio_dev, int cpu_id)
{
	int sdest, ret;

	/* Set the Stashing Destination */
	if (cpu_id < 0) {
		cpu_id = rte_get_master_lcore();
		if (cpu_id < 0) {
			DPAA2_BUS_ERR("Getting CPU Index failed");
			return -1;
		}
	}
	/* Set the STASH Destination depending on Current CPU ID.
	 * Valid values of SDEST are 4,5,6,7. Where,
	 */

	sdest = dpaa2_core_cluster_sdest(cpu_id);
	DPAA2_BUS_DEBUG("Portal= %d  CPU= %u SDEST= %d",
			dpio_dev->index, cpu_id, sdest);

	ret = dpio_set_stashing_destination(dpio_dev->dpio, CMD_PRI_LOW,
					    dpio_dev->token, sdest);
	if (ret) {
		DPAA2_BUS_ERR("%d ERROR in SDEST",  ret);
		return -1;
	}

#ifdef RTE_LIBRTE_PMD_DPAA2_EVENTDEV
	if (dpaa2_dpio_intr_init(dpio_dev)) {
		DPAA2_BUS_ERR("Interrupt registration failed for dpio");
		return -1;
	}
#endif

	return 0;
}

struct dpaa2_dpio_dev *dpaa2_get_qbman_swp(int cpu_id)
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

	ret = dpaa2_configure_stashing(dpio_dev, cpu_id);
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

static int
dpaa2_create_dpio_device(int vdev_fd,
			 struct vfio_device_info *obj_info,
			 int object_id)
{
	struct dpaa2_dpio_dev *dpio_dev;
	struct vfio_region_info reg_info = { .argsz = sizeof(reg_info)};

	if (obj_info->num_regions < NUM_DPIO_REGIONS) {
		DPAA2_BUS_ERR("Not sufficient number of DPIO regions");
		return -1;
	}

	dpio_dev = rte_malloc(NULL, sizeof(struct dpaa2_dpio_dev),
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

	reg_info.index = 0;
	if (ioctl(vdev_fd, VFIO_DEVICE_GET_REGION_INFO, &reg_info)) {
		DPAA2_BUS_ERR("vfio: error getting region info");
		rte_free(dpio_dev);
		return -1;
	}

	dpio_dev->ce_size = reg_info.size;
	dpio_dev->qbman_portal_ce_paddr = (size_t)mmap(NULL, reg_info.size,
				PROT_WRITE | PROT_READ, MAP_SHARED,
				vdev_fd, reg_info.offset);

	reg_info.index = 1;
	if (ioctl(vdev_fd, VFIO_DEVICE_GET_REGION_INFO, &reg_info)) {
		DPAA2_BUS_ERR("vfio: error getting region info");
		rte_free(dpio_dev);
		return -1;
	}

	dpio_dev->ci_size = reg_info.size;
	dpio_dev->qbman_portal_ci_paddr = (size_t)mmap(NULL, reg_info.size,
				PROT_WRITE | PROT_READ, MAP_SHARED,
				vdev_fd, reg_info.offset);

	if (configure_dpio_qbman_swp(dpio_dev)) {
		DPAA2_BUS_ERR(
			     "Fail to configure the dpio qbman portal for %d",
			     dpio_dev->hw_id);
		rte_free(dpio_dev);
		return -1;
	}

	io_space_count++;
	dpio_dev->index = io_space_count;

	if (rte_dpaa2_vfio_setup_intr(&dpio_dev->intr_handle, vdev_fd, 1)) {
		DPAA2_BUS_ERR("Fail to setup interrupt for %d",
			      dpio_dev->hw_id);
		rte_free(dpio_dev);
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
	}

	TAILQ_INSERT_TAIL(&dpio_dev_list, dpio_dev, next);

	return 0;
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
			DPAA2_DQRR_RING_SIZE * sizeof(struct qbman_result),
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

static struct rte_dpaa2_object rte_dpaa2_dpio_obj = {
	.dev_type = DPAA2_IO,
	.create = dpaa2_create_dpio_device,
};

RTE_PMD_REGISTER_DPAA2_OBJECT(dpio, rte_dpaa2_dpio_obj);
