/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2013-2015 Freescale Semiconductor Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 *   GPL LICENSE SUMMARY
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
#include <libio.h>
#include <stdint.h>
#include <errno.h>
#include <sys/uio.h>
#include <linux/byteorder/little_endian.h>

#define cpu_to_le64(x) __cpu_to_le64(x)
#ifndef dmb
#define dmb() {__asm__ __volatile__("" : : : "memory"); }
#endif
#define __iormb()       dmb()
#define __iowmb()       dmb()
#define __arch_getq(a)                  (*(volatile unsigned long *)(a))
#define __arch_putq(v, a)                (*(volatile unsigned long *)(a) = (v))
#define __arch_putq32(v, a)                (*(volatile unsigned int *)(a) = (v))
#define readq(c)        \
	({ uint64_t __v = __arch_getq(c); __iormb(); __v; })
#define writeq(v, c)     \
	({ uint64_t __v = v; __iowmb(); __arch_putq(__v, c); __v; })
#define writeq32(v, c) \
	({ uint32_t __v = v; __iowmb(); __arch_putq32(__v, c); __v; })
#define ioread64(_p)	    readq(_p)
#define iowrite64(_v, _p)   writeq(_v, _p)
#define iowrite32(_v, _p)   writeq32(_v, _p)
#define __iomem

struct fsl_mc_io {
	void *regs;
};

#ifndef ENOTSUP
#define ENOTSUP		95
#endif

/*GPP is supposed to use MC commands with low priority*/
#define CMD_PRI_LOW          0 /*!< Low Priority command indication */

struct mc_command;

int mc_send_command(struct fsl_mc_io *mc_io, struct mc_command *cmd);

#endif /* __linux_driver__ */

#endif /* _FSL_MC_SYS_H */
