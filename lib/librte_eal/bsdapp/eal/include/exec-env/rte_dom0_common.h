/*-
 *   This file is provided under a dual BSD/LGPLv2 license.  When using or
 *   redistributing this file, you may do so under either license.
 *
 *   GNU LESSER GENERAL PUBLIC LICENSE
 *
 *   Copyright(c) 2007-2014 Intel Corporation. All rights reserved.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of version 2.1 of the GNU Lesser General Public License
 *   as published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *   Contact Information:
 *   Intel Corporation
 *
 *
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _RTE_DOM0_COMMON_H_
#define _RTE_DOM0_COMMON_H_

#ifdef __KERNEL__
#include <linux/if.h>
#endif

#define DOM0_NAME_MAX   256
#define DOM0_MM_DEV   "/dev/dom0_mm"

#define DOM0_CONTIG_NUM_ORDER       9       /**< 2M order */
#define DOM0_NUM_MEMSEG             512     /**< Maximum nb. of memory segment. */
#define DOM0_MEMBLOCK_SIZE          0x200000 /**< Maximum nb. of memory block(2M). */
#define DOM0_CONFIG_MEMSIZE         4096     /**< Maximum config memory size(4G). */
#define DOM0_NUM_MEMBLOCK (DOM0_CONFIG_MEMSIZE / 2) /**< Maximum nb. of 2M memory block. */

#define RTE_DOM0_IOCTL_PREPARE_MEMSEG    _IOWR(0, 1 , struct memory_info)
#define RTE_DOM0_IOCTL_ATTACH_TO_MEMSEG  _IOWR(0, 2 , char *)
#define RTE_DOM0_IOCTL_GET_NUM_MEMSEG    _IOWR(0, 3, int)
#define RTE_DOM0_IOCTL_GET_MEMSEG_INFO   _IOWR(0, 4, void *)

/**
 * A structure used to store memory information.
 */
struct memory_info {
	char name[DOM0_NAME_MAX];
	uint64_t size;
};

/**
 * A structure used to store memory segment information.
 */
struct memseg_info {
	uint32_t idx;
	uint64_t pfn;
	uint64_t size;
	uint64_t mfn[DOM0_NUM_MEMBLOCK];
};

/**
 * A structure used to store memory block information.
 */
struct memblock_info {
	uint8_t  exchange_flag;
	uint64_t vir_addr;
	uint64_t pfn;
	uint64_t mfn;
};
#endif /* _RTE_DOM0_COMMON_H_ */
