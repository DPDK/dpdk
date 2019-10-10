/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2017 NXP
 *
 */

#ifndef __DPAA_SYS_H
#define __DPAA_SYS_H

#include <compat.h>
#include <dpaa_of.h>

/* For 2-element tables related to cache-inhibited and cache-enabled mappings */
#define DPAA_PORTAL_CE 0
#define DPAA_PORTAL_CI 1

#define DPAA_ASSERT(x) RTE_ASSERT(x)

/* This is the interface from the platform-agnostic driver code to (de)register
 * interrupt handlers. We simply create/destroy corresponding structs.
 */
int qbman_request_irq(int irq, irqreturn_t (*isr)(int irq, void *arg),
		      unsigned long flags, const char *name, void *arg);
int qbman_free_irq(int irq, void *arg);

void qbman_invoke_irq(int irq);

#endif /* __DPAA_SYS_H */
