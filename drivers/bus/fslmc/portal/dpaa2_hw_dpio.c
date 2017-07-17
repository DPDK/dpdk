/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016 NXP.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Freescale Semiconductor, Inc nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_dev.h>

#include <fslmc_logs.h>
#include <fslmc_vfio.h>
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

/* Set the STASH Destination depending on Current CPU ID.
 * e.g. Valid values of SDEST are 4,5,6,7. Where,
 * CPU 0-1 will have SDEST 4
 * CPU 2-3 will have SDEST 5.....and so on.
 */
static int
dpaa2_core_cluster_sdest(int cpu_id)
{
	int x = cpu_id / dpaa2_cluster_sz;

	if (x > 3)
		x = 3;

	return dpaa2_core_cluster_base + x;
}

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
		PMD_DRV_LOG(WARNING, "Failed to open /proc/interrupts file\n");
		return;
	}
	while (getline(&temp, &len, file) != -1) {
		if ((strstr(temp, string)) != NULL) {
			token = strtok(temp, ":");
			break;
		}
	}

	if (!token) {
		PMD_DRV_LOG(WARNING, "Failed to get interrupt id for dpio.%d\n",
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
		PMD_DRV_LOG(WARNING,
			"Failed to affine interrupts on respective core\n");
	else
		PMD_DRV_LOG(WARNING, " %s command is executed\n", command);

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
		PMD_DRV_LOG(ERR, "Interrupt registeration failed\n");
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
		PMD_DRV_LOG(ERR, "epoll_ctl failed\n");
		return -1;
	}
	dpio_dev->epoll_fd = dpio_epoll_fd;

	dpaa2_affine_dpio_intr_to_respective_core(dpio_dev->hw_id);

	return 0;
}

static int
configure_dpio_qbman_swp(struct dpaa2_dpio_dev *dpio_dev)
{
	struct qbman_swp_desc p_des;
	struct dpio_attr attr;

	dpio_dev->dpio = malloc(sizeof(struct fsl_mc_io));
	if (!dpio_dev->dpio) {
		PMD_INIT_LOG(ERR, "Memory allocation failure\n");
		return -1;
	}

	PMD_DRV_LOG(DEBUG, "\t Allocated  DPIO Portal[%p]", dpio_dev->dpio);
	dpio_dev->dpio->regs = dpio_dev->mc_portal;
	if (dpio_open(dpio_dev->dpio, CMD_PRI_LOW, dpio_dev->hw_id,
		      &dpio_dev->token)) {
		PMD_INIT_LOG(ERR, "Failed to allocate IO space\n");
		free(dpio_dev->dpio);
		return -1;
	}

	if (dpio_reset(dpio_dev->dpio, CMD_PRI_LOW, dpio_dev->token)) {
		PMD_INIT_LOG(ERR, "Failed to reset dpio\n");
		dpio_close(dpio_dev->dpio, CMD_PRI_LOW, dpio_dev->token);
		free(dpio_dev->dpio);
		return -1;
	}

	if (dpio_enable(dpio_dev->dpio, CMD_PRI_LOW, dpio_dev->token)) {
		PMD_INIT_LOG(ERR, "Failed to Enable dpio\n");
		dpio_close(dpio_dev->dpio, CMD_PRI_LOW, dpio_dev->token);
		free(dpio_dev->dpio);
		return -1;
	}

	if (dpio_get_attributes(dpio_dev->dpio, CMD_PRI_LOW,
				dpio_dev->token, &attr)) {
		PMD_INIT_LOG(ERR, "DPIO Get attribute failed\n");
		dpio_disable(dpio_dev->dpio, CMD_PRI_LOW, dpio_dev->token);
		dpio_close(dpio_dev->dpio, CMD_PRI_LOW,  dpio_dev->token);
		free(dpio_dev->dpio);
		return -1;
	}

	PMD_INIT_LOG(DEBUG, "Qbman Portal ID %d", attr.qbman_portal_id);

	/* Configure & setup SW portal */
	p_des.block = NULL;
	p_des.idx = attr.qbman_portal_id;
	p_des.cena_bar = (void *)(dpio_dev->qbman_portal_ce_paddr);
	p_des.cinh_bar = (void *)(dpio_dev->qbman_portal_ci_paddr);
	p_des.irq = -1;
	p_des.qman_version = attr.qbman_version;

	dpio_dev->sw_portal = qbman_swp_init(&p_des);
	if (dpio_dev->sw_portal == NULL) {
		PMD_DRV_LOG(ERR, " QBMan SW Portal Init failed\n");
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
	static int first_time;

	/* find the SoC type for the first time */
	if (!first_time) {
		struct mc_soc_version mc_plat_info = {0};

		if (mc_get_soc_version(dpio_dev->dpio,
				       CMD_PRI_LOW, &mc_plat_info)) {
			PMD_INIT_LOG(ERR, "\tmc_get_soc_version failed\n");
		} else if ((mc_plat_info.svr & 0xffff0000) == SVR_LS1080A) {
			dpaa2_core_cluster_base = 0x02;
			dpaa2_cluster_sz = 4;
			PMD_INIT_LOG(DEBUG, "\tLS108x (A53) Platform Detected");
		}
		first_time = 1;
	}

	/* Set the Stashing Destination */
	if (cpu_id < 0) {
		cpu_id = rte_get_master_lcore();
		if (cpu_id < 0) {
			RTE_LOG(ERR, PMD, "\tGetting CPU Index failed\n");
			return -1;
		}
	}
	/* Set the STASH Destination depending on Current CPU ID.
	 * Valid values of SDEST are 4,5,6,7. Where,
	 */

	sdest = dpaa2_core_cluster_sdest(cpu_id);
	PMD_DRV_LOG(DEBUG, "Portal= %d  CPU= %u SDEST= %d",
		    dpio_dev->index, cpu_id, sdest);

	ret = dpio_set_stashing_destination(dpio_dev->dpio, CMD_PRI_LOW,
					    dpio_dev->token, sdest);
	if (ret) {
		PMD_DRV_LOG(ERR, "%d ERROR in SDEST\n",  ret);
		return -1;
	}

	if (dpaa2_dpio_intr_init(dpio_dev)) {
		PMD_DRV_LOG(ERR, "Interrupt registration failed for dpio\n");
		return -1;
	}

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

	PMD_DRV_LOG(DEBUG, "New Portal=0x%x (%d) affined thread - %lu",
		    dpio_dev, dpio_dev->index, syscall(SYS_gettid));

	ret = dpaa2_configure_stashing(dpio_dev, cpu_id);
	if (ret)
		PMD_DRV_LOG(ERR, "dpaa2_configure_stashing failed");

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
		PMD_DRV_LOG(INFO, "DPAA Portal=0x%x (%d) is being shared"
			    " between thread %lu and current  %lu",
			    dpaa2_io_portal[lcore_id].dpio_dev,
			    dpaa2_io_portal[lcore_id].dpio_dev->index,
			    dpaa2_io_portal[lcore_id].net_tid,
			    tid);
		RTE_PER_LCORE(_dpaa2_io).dpio_dev
			= dpaa2_io_portal[lcore_id].dpio_dev;
		rte_atomic16_inc(&dpaa2_io_portal
				 [lcore_id].dpio_dev->ref_count);
		dpaa2_io_portal[lcore_id].net_tid = tid;

		PMD_DRV_LOG(DEBUG, "Old Portal=0x%x (%d) affined thread - %lu",
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
dpaa2_affine_qbman_swp_sec(void)
{
	unsigned int lcore_id = rte_lcore_id();
	uint64_t tid = syscall(SYS_gettid);

	if (lcore_id == LCORE_ID_ANY)
		lcore_id = rte_get_master_lcore();
	/* if the core id is not supported */
	else if (lcore_id >= RTE_MAX_LCORE)
		return -1;

	if (dpaa2_io_portal[lcore_id].sec_dpio_dev) {
		PMD_DRV_LOG(INFO, "DPAA Portal=0x%x (%d) is being shared"
			    " between thread %lu and current  %lu",
			    dpaa2_io_portal[lcore_id].sec_dpio_dev,
			    dpaa2_io_portal[lcore_id].sec_dpio_dev->index,
			    dpaa2_io_portal[lcore_id].sec_tid,
			    tid);
		RTE_PER_LCORE(_dpaa2_io).sec_dpio_dev
			= dpaa2_io_portal[lcore_id].sec_dpio_dev;
		rte_atomic16_inc(&dpaa2_io_portal
				 [lcore_id].sec_dpio_dev->ref_count);
		dpaa2_io_portal[lcore_id].sec_tid = tid;

		PMD_DRV_LOG(DEBUG, "Old Portal=0x%x (%d) affined thread - %lu",
			    dpaa2_io_portal[lcore_id].sec_dpio_dev,
			    dpaa2_io_portal[lcore_id].sec_dpio_dev->index,
			    tid);
		return 0;
	}

	/* Populate the dpaa2_io_portal structure */
	dpaa2_io_portal[lcore_id].sec_dpio_dev = dpaa2_get_qbman_swp(lcore_id);

	if (dpaa2_io_portal[lcore_id].sec_dpio_dev) {
		RTE_PER_LCORE(_dpaa2_io).sec_dpio_dev
			= dpaa2_io_portal[lcore_id].sec_dpio_dev;
		dpaa2_io_portal[lcore_id].sec_tid = tid;
		return 0;
	} else {
		return -1;
	}
}

static int
dpaa2_create_dpio_device(struct fslmc_vfio_device *vdev,
			 struct vfio_device_info *obj_info,
			 int object_id)
{
	struct dpaa2_dpio_dev *dpio_dev;
	struct vfio_region_info reg_info = { .argsz = sizeof(reg_info)};
	int vfio_dev_fd;

	if (obj_info->num_regions < NUM_DPIO_REGIONS) {
		PMD_INIT_LOG(ERR, "ERROR, Not sufficient number "
				"of DPIO regions.\n");
		return -1;
	}

	dpio_dev = rte_malloc(NULL, sizeof(struct dpaa2_dpio_dev),
			      RTE_CACHE_LINE_SIZE);
	if (!dpio_dev) {
		PMD_INIT_LOG(ERR, "Memory allocation failed for DPIO Device\n");
		return -1;
	}

	dpio_dev->dpio = NULL;
	dpio_dev->hw_id = object_id;
	dpio_dev->intr_handle.vfio_dev_fd = vdev->fd;
	rte_atomic16_init(&dpio_dev->ref_count);
	/* Using single portal  for all devices */
	dpio_dev->mc_portal = rte_mcp_ptr_list[MC_PORTAL_INDEX];

	reg_info.index = 0;
	vfio_dev_fd = dpio_dev->intr_handle.vfio_dev_fd;
	if (ioctl(vfio_dev_fd, VFIO_DEVICE_GET_REGION_INFO, &reg_info)) {
		PMD_INIT_LOG(ERR, "vfio: error getting region info\n");
		rte_free(dpio_dev);
		return -1;
	}

	dpio_dev->ce_size = reg_info.size;
	dpio_dev->qbman_portal_ce_paddr = (uint64_t)mmap(NULL, reg_info.size,
				PROT_WRITE | PROT_READ, MAP_SHARED,
				vfio_dev_fd, reg_info.offset);

	reg_info.index = 1;
	if (ioctl(vfio_dev_fd, VFIO_DEVICE_GET_REGION_INFO, &reg_info)) {
		PMD_INIT_LOG(ERR, "vfio: error getting region info\n");
		rte_free(dpio_dev);
		return -1;
	}

	dpio_dev->ci_size = reg_info.size;
	dpio_dev->qbman_portal_ci_paddr = (uint64_t)mmap(NULL, reg_info.size,
				PROT_WRITE | PROT_READ, MAP_SHARED,
				vfio_dev_fd, reg_info.offset);

	if (configure_dpio_qbman_swp(dpio_dev)) {
		PMD_INIT_LOG(ERR,
			     "Fail to configure the dpio qbman portal for %d\n",
			     dpio_dev->hw_id);
		rte_free(dpio_dev);
		return -1;
	}

	io_space_count++;
	dpio_dev->index = io_space_count;
	TAILQ_INSERT_TAIL(&dpio_dev_list, dpio_dev, next);
	PMD_INIT_LOG(DEBUG, "DPAA2: Added [dpio.%d]", object_id);

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
	.object_id = DPAA2_MC_DPIO_DEVID,
	.create = dpaa2_create_dpio_device,
};

RTE_PMD_REGISTER_DPAA2_OBJECT(dpio, rte_dpaa2_dpio_obj);
