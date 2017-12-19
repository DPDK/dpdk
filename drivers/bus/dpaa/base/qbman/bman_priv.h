/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2017 NXP
 *
 */

#ifndef __BMAN_PRIV_H
#define __BMAN_PRIV_H

#include "dpaa_sys.h"
#include <fsl_bman.h>

/* Revision info (for errata and feature handling) */
#define BMAN_REV10 0x0100
#define BMAN_REV20 0x0200
#define BMAN_REV21 0x0201

#define BMAN_PORTAL_IRQ_PATH "/dev/fsl-usdpaa-irq"
#define BMAN_CCSR_MAP "/dev/mem"

/* This mask contains all the "irqsource" bits visible to API users */
#define BM_PIRQ_VISIBLE	(BM_PIRQ_RCRI | BM_PIRQ_BSCN)

/* These are bm_<reg>_<verb>(). So for example, bm_disable_write() means "write
 * the disable register" rather than "disable the ability to write".
 */
#define bm_isr_status_read(bm)		__bm_isr_read(bm, bm_isr_status)
#define bm_isr_status_clear(bm, m)	__bm_isr_write(bm, bm_isr_status, m)
#define bm_isr_enable_read(bm)		__bm_isr_read(bm, bm_isr_enable)
#define bm_isr_enable_write(bm, v)	__bm_isr_write(bm, bm_isr_enable, v)
#define bm_isr_disable_read(bm)		__bm_isr_read(bm, bm_isr_disable)
#define bm_isr_disable_write(bm, v)	__bm_isr_write(bm, bm_isr_disable, v)
#define bm_isr_inhibit(bm)		__bm_isr_write(bm, bm_isr_inhibit, 1)
#define bm_isr_uninhibit(bm)		__bm_isr_write(bm, bm_isr_inhibit, 0)

/*
 * Global variables of the max portal/pool number this bman version supported
 */
extern u16 bman_pool_max;

/* used by CCSR and portal interrupt code */
enum bm_isr_reg {
	bm_isr_status = 0,
	bm_isr_enable = 1,
	bm_isr_disable = 2,
	bm_isr_inhibit = 3
};

struct bm_portal_config {
	/*
	 * Corenet portal addresses;
	 * [0]==cache-enabled, [1]==cache-inhibited.
	 */
	void __iomem *addr_virt[2];
	/* Allow these to be joined in lists */
	struct list_head list;
	/* User-visible portal configuration settings */
	/* This is used for any "core-affine" portals, ie. default portals
	 * associated to the corresponding cpu. -1 implies that there is no
	 * core affinity configured.
	 */
	int cpu;
	/* portal interrupt line */
	int irq;
	/* the unique index of this portal */
	u32 index;
	/* Is this portal shared? (If so, it has coarser locking and demuxes
	 * processing on behalf of other CPUs.).
	 */
	int is_shared;
	/* These are the buffer pool IDs that may be used via this portal. */
	struct bman_depletion mask;

};

int bman_init_ccsr(const struct device_node *node);

struct bman_portal *bman_create_affine_portal(
			const struct bm_portal_config *config);
const struct bm_portal_config *bman_destroy_affine_portal(void);

/* Set depletion thresholds associated with a buffer pool. Requires that the
 * operating system have access to Bman CCSR (ie. compiled in support and
 * run-time access courtesy of the device-tree).
 */
int bm_pool_set(u32 bpid, const u32 *thresholds);

/* Read the free buffer count for a given buffer */
u32 bm_pool_free_buffers(u32 bpid);

#endif /* __BMAN_PRIV_H */
