/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016 NXP
 *
 */

#ifndef _DPAA2_HW_DPIO_H_
#define _DPAA2_HW_DPIO_H_

#include <mc/fsl_dpio.h>
#include <mc/fsl_mc_sys.h>

struct dpaa2_io_portal_t {
	struct dpaa2_dpio_dev *dpio_dev;
	struct dpaa2_dpio_dev *ethrx_dpio_dev;
	uint64_t net_tid;
	uint64_t sec_tid;
	void *eventdev;
};

/*! Global per thread DPIO portal */
RTE_DECLARE_PER_LCORE(struct dpaa2_io_portal_t, _dpaa2_io);

#define DPAA2_PER_LCORE_DPIO RTE_PER_LCORE(_dpaa2_io).dpio_dev
#define DPAA2_PER_LCORE_PORTAL DPAA2_PER_LCORE_DPIO->sw_portal

#define DPAA2_PER_LCORE_ETHRX_DPIO RTE_PER_LCORE(_dpaa2_io).ethrx_dpio_dev
#define DPAA2_PER_LCORE_ETHRX_PORTAL DPAA2_PER_LCORE_ETHRX_DPIO->sw_portal

/* Variable to store DPAA2 platform type */
extern uint32_t dpaa2_svr_family;
/* Variable to store DPAA2 DQRR size */
extern uint8_t dpaa2_dqrr_size;
/* Variable to store DPAA2 EQCR size */
extern uint8_t dpaa2_eqcr_size;

extern struct dpaa2_io_portal_t dpaa2_io_portal[RTE_MAX_LCORE];

/* Affine a DPIO portal to current processing thread */
int dpaa2_affine_qbman_swp(void);

/* Affine additional DPIO portal to current crypto processing thread */
int dpaa2_affine_qbman_ethrx_swp(void);

/* allocate memory for FQ - dq storage */
int
dpaa2_alloc_dq_storage(struct queue_storage_info_t *q_storage);

/* free memory for FQ- dq storage */
void
dpaa2_free_dq_storage(struct queue_storage_info_t *q_storage);

/* free the enqueue response descriptors */
uint32_t
dpaa2_free_eq_descriptors(void);

#endif /* _DPAA2_HW_DPIO_H_ */
