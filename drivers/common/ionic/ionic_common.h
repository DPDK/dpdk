/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2024 Advanced Micro Devices, Inc.
 */

#ifndef _IONIC_COMMON_H_
#define _IONIC_COMMON_H_

#include <stdint.h>
#include <assert.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_eal_paging.h>

#include "ionic_osdep.h"

#define IONIC_DEVCMD_TIMEOUT		5	/* devcmd_timeout */
#define IONIC_DEVCMD_CHECK_PERIOD_US	10	/* devcmd status chk period */
#define IONIC_DEVCMD_RETRY_WAIT_US	20000

#define IONIC_Q_WDOG_MS			10	/* 10ms */
#define IONIC_Q_WDOG_MAX_MS		5000	/* 5s */
#define IONIC_ADMINQ_WDOG_MS		500	/* 500ms */

#define IONIC_ALIGN			4096

struct ionic_dev_bar {
	void __iomem *vaddr;
	rte_iova_t bus_addr;
	unsigned long len;
};

__rte_internal
void ionic_uio_scan_mnet_devices(void);
__rte_internal
void ionic_uio_scan_mcrypt_devices(void);

__rte_internal
void ionic_uio_get_rsrc(const char *name, int idx, struct ionic_dev_bar *bar);
__rte_internal
void ionic_uio_rel_rsrc(const char *name, int idx, struct ionic_dev_bar *bar);

#endif /* _IONIC_COMMON_H_ */
