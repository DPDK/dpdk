/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2015-2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016,2019 NXP
 *
 */

#ifndef _FSLMC_VFIO_H_
#define _FSLMC_VFIO_H_

#include <rte_compat.h>
#include <rte_vfio.h>

/* Pathname of FSL-MC devices directory. */
#define SYSFS_FSL_MC_DEVICES	"/sys/bus/fsl-mc/devices"
#define DPAA2_MC_DPNI_DEVID	7
#define DPAA2_MC_DPSECI_DEVID	3
#define DPAA2_MC_DPCON_DEVID	5
#define DPAA2_MC_DPIO_DEVID	9
#define DPAA2_MC_DPBP_DEVID	10
#define DPAA2_MC_DPCI_DEVID	11

typedef struct fslmc_vfio_device {
	int fd; /* fslmc root container device ?? */
	int index; /*index of child object */
	struct fslmc_vfio_device *child; /* Child object */
} fslmc_vfio_device;

typedef struct fslmc_vfio_group {
	int fd; /* /dev/vfio/"groupid" */
	int groupid;
	struct fslmc_vfio_container *container;
	int object_index;
	struct fslmc_vfio_device *vfio_device;
} fslmc_vfio_group;

typedef struct fslmc_vfio_container {
	int fd; /* /dev/vfio/vfio */
	int used;
	int index; /* index in group list */
	struct fslmc_vfio_group *group;
} fslmc_vfio_container;

extern char *fslmc_container;

__rte_internal
int rte_dpaa2_intr_enable(struct rte_intr_handle *intr_handle, int index);

__rte_internal
int rte_dpaa2_intr_disable(struct rte_intr_handle *intr_handle, int index);

int rte_dpaa2_vfio_setup_intr(struct rte_intr_handle *intr_handle,
			      int vfio_dev_fd,
			      int num_irqs);

int fslmc_vfio_setup_group(void);
int fslmc_vfio_process_group(void);
char *fslmc_get_container(void);
int fslmc_get_container_group(int *gropuid);
int rte_fslmc_vfio_dmamap(void);
__rte_experimental
int rte_fslmc_vfio_mem_dmamap(uint64_t vaddr, uint64_t iova, uint64_t size);

#endif /* _FSLMC_VFIO_H_ */
