/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2021 Pensando Systems, Inc. All rights reserved.
 */

#ifndef _IONIC_COMMON_H_
#define _IONIC_COMMON_H_

#include <stdint.h>
#include <assert.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_io.h>
#include <rte_memory.h>
#include <rte_cycles.h>

#define BIT(nr)            (1UL << (nr))
#define BIT_ULL(nr)        (1ULL << (nr))

#define IONIC_DEVCMD_TIMEOUT            5       /* devcmd_timeout */
#define IONIC_DEVCMD_CHECK_PERIOD_US    10      /* devcmd status chk period */
#define IONIC_DEVCMD_RETRY_WAIT_US      20000

#define IONIC_Q_WDOG_MS                 10      /* 10ms */
#define IONIC_Q_WDOG_MAX_MS             5000    /* 5s */
#define IONIC_ADMINQ_WDOG_MS            500     /* 500ms */

#define IONIC_ALIGN                     4096

#define __iomem

#ifndef __rte_cold
#define __rte_cold __attribute__((cold))
#endif

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#ifndef __le16
#define __le16 uint16_t
#endif
#ifndef __le32
#define __le32 uint32_t
#endif
#ifndef __le64
#define __le64 uint64_t
#endif

#define ioread8(reg)            rte_read8(reg)
#define ioread32(reg)           rte_read32(rte_le_to_cpu_32(reg))
#define iowrite8(value, reg)    rte_write8(value, reg)
#define iowrite32(value, reg)   rte_write32(rte_cpu_to_le_32(value), reg)

struct ionic_dev_bar {
	void __iomem *vaddr;
	rte_iova_t bus_addr;
	unsigned long len;
};

void ionic_uio_scan_mnet_devices(void);
void ionic_uio_scan_mcrypt_devices(void);

void ionic_uio_get_rsrc(const char *name, int idx, struct ionic_dev_bar *bar);
void ionic_uio_rel_rsrc(const char *name, int idx, struct ionic_dev_bar *bar);

//size_t rte_mem_page_size(void);

#endif /* _IONIC_COMMON_H_ */
