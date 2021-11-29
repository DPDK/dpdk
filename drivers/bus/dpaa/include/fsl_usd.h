/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2010-2011 Freescale Semiconductor, Inc.
 * All rights reserved.
 * Copyright 2019 NXP
 *
 */

#ifndef __FSL_USD_H
#define __FSL_USD_H

#include <compat.h>
#include <dpaa_list.h>
#include <fsl_qman.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Thread-entry/exit hooks; */
int qman_thread_init(void);
int bman_thread_init(void);
int qman_thread_finish(void);
int bman_thread_finish(void);

#define QBMAN_ANY_PORTAL_IDX 0xffffffff

/* Obtain and free raw (unitialized) portals */

struct dpaa_raw_portal {
	/* inputs */

	/* set to non zero to turn on stashing */
	uint8_t enable_stash;
	/* Stashing attributes for the portal */
	uint32_t cpu;
	uint32_t cache;
	uint32_t window;

	/* Specifies the stash request queue this portal should use */
	uint8_t sdest;

	/* Specifies a specific portal index to map or QBMAN_ANY_PORTAL_IDX
	 * for don't care.  The portal index will be populated by the
	 * driver when the ioctl() successfully completes.
	 */
	uint32_t index;

	/* outputs */
	uint64_t cinh;
	uint64_t cena;
};

int qman_allocate_raw_portal(struct dpaa_raw_portal *portal);
int qman_free_raw_portal(struct dpaa_raw_portal *portal);

int bman_allocate_raw_portal(struct dpaa_raw_portal *portal);
int bman_free_raw_portal(struct dpaa_raw_portal *portal);

/* Obtain thread-local UIO file-descriptors */
__rte_internal
int qman_thread_fd(void);
int bman_thread_fd(void);

/* Post-process interrupts. NB, the kernel IRQ handler disables the interrupt
 * line before notifying us, and this post-processing re-enables it once
 * processing is complete. As such, it is essential to call this before going
 * into another blocking read/select/poll.
 */
__rte_internal
void qman_thread_irq(void);

__rte_internal
void bman_thread_irq(void);
__rte_internal
void qman_fq_portal_thread_irq(struct qman_portal *qp);
__rte_internal
void qman_clear_irq(void);

/* Global setup */
int qman_global_init(void);
int bman_global_init(void);

/* Direct portal create and destroy */
__rte_internal
struct qman_portal *fsl_qman_fq_portal_create(int *fd);
int fsl_qman_fq_portal_destroy(struct qman_portal *qp);
int fsl_qman_fq_portal_init(struct qman_portal *qp);

#ifdef __cplusplus
}
#endif

#endif /* __FSL_USD_H */
