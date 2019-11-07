/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2015 Freescale Semiconductor Inc.
 * Copyright 2017 NXP
 *
 */
#ifndef _FSL_MC_SYS_H
#define _FSL_MC_SYS_H

#ifdef __linux_driver__

#include <linux/errno.h>
#include <asm/io.h>
#include <linux/slab.h>

struct fsl_mc_io {
	void *regs;
};

#ifndef ENOTSUP
#define ENOTSUP		95
#endif

#define ioread64(_p)	    readq(_p)
#define iowrite64(_v, _p)   writeq(_v, _p)

#else /* __linux_driver__ */

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <sys/uio.h>
#include <linux/byteorder/little_endian.h>

#include <rte_atomic.h>

#define __iormb()	rte_io_rmb()
#define __iowmb()	rte_io_wmb()
#define __arch_getq(a)		(*(volatile uint64_t *)(a))
#define __arch_putq(v, a)	(*(volatile uint64_t *)(a) = (v))
#define __arch_putq32(v, a)	(*(volatile uint32_t *)(a) = (v))
#define readq(c) \
	({ uint64_t __v = __arch_getq(c); __iormb(); __v; })
#define writeq(v, c) \
	({ uint64_t __v = v; __iowmb(); __arch_putq(__v, c); __v; })
#define writeq32(v, c) \
	({ uint32_t __v = v; __iowmb(); __arch_putq32(__v, c); __v; })
#define ioread64(_p)		readq(_p)
#define iowrite64(_v, _p)	writeq(_v, _p)
#define iowrite32(_v, _p)	writeq32(_v, _p)
#define __iomem

/*GPP is supposed to use MC commands with low priority*/
#define CMD_PRI_LOW          0 /*!< Low Priority command indication */

struct fsl_mc_io {
	void *regs;
};

#endif /* __linux_driver__ */

#endif /* _FSL_MC_SYS_H */
