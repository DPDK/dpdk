/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2015-2016 Freescale Semiconductor, Inc. All rights reserved.
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

#ifndef _FSLMC_VFIO_H_
#define _FSLMC_VFIO_H_

#include "eal_vfio.h"

#define DPAA2_VENDOR_ID		0x1957
#define DPAA2_MC_DPNI_DEVID	7
#define DPAA2_MC_DPSECI_DEVID	3
#define DPAA2_MC_DPCON_DEVID	5
#define DPAA2_MC_DPIO_DEVID	9
#define DPAA2_MC_DPBP_DEVID	10
#define DPAA2_MC_DPCI_DEVID	11

#define VFIO_MAX_GRP 1

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
	struct fslmc_vfio_group *group_list[VFIO_MAX_GRP];
} fslmc_vfio_container;

struct rte_dpaa2_object;

TAILQ_HEAD(rte_fslmc_object_list, rte_dpaa2_object);

typedef int (*rte_fslmc_obj_create_t)(struct fslmc_vfio_device *vdev,
					 struct vfio_device_info *obj_info,
					 int object_id);

/**
 * A structure describing a DPAA2 driver.
 */
struct rte_dpaa2_object {
	TAILQ_ENTRY(rte_dpaa2_object) next; /**< Next in list. */
	const char *name;            /**< Name of Object. */
	uint16_t object_id;             /**< DPAA2 Object ID */
	rte_fslmc_obj_create_t create;
};

int rte_dpaa2_intr_enable(struct rte_intr_handle *intr_handle,
			  uint32_t index);

int fslmc_vfio_setup_group(void);
int fslmc_vfio_process_group(void);
int rte_fslmc_vfio_dmamap(void);

/**
 * Register a DPAA2 MC Object driver.
 *
 * @param mc_object
 *   A pointer to a rte_dpaa_object structure describing the mc object
 *   to be registered.
 */
void rte_fslmc_object_register(struct rte_dpaa2_object *object);

/** Helper for DPAA2 object registration */
#define RTE_PMD_REGISTER_DPAA2_OBJECT(nm, dpaa2_obj) \
RTE_INIT(dpaa2objinitfn_ ##nm); \
static void dpaa2objinitfn_ ##nm(void) \
{\
	(dpaa2_obj).name = RTE_STR(nm);\
	rte_fslmc_object_register(&dpaa2_obj); \
} \
RTE_PMD_EXPORT_NAME(nm, __COUNTER__)

#endif /* _FSLMC_VFIO_H_ */
