/*-
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 *   redistributing this file, you may do so under either license.
 *
 *   GPL LICENSE SUMMARY
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *   The full GNU General Public License is included in this distribution
 *   in the file called LICENSE.GPL.
 *
 *   Contact Information:
 *   Intel Corporation
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
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
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
 *
 */
#ifndef _DOM0_MM_DEV_H_
#define _DOM0_MM_DEV_H_

#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <exec-env/rte_dom0_common.h>

#define NUM_MEM_CTX     256  /**< Maximum number of memory context*/
#define MAX_EXCHANGE_FAIL_TIME 5  /**< Maximum times of allowing exchange fail .*/
#define MAX_MEMBLOCK_SIZE (2 * DOM0_MEMBLOCK_SIZE)
#define MAX_NUM_ORDER     (DOM0_CONTIG_NUM_ORDER + 1)
#define SIZE_PER_BLOCK    2       /**< Size of memory block (2MB).*/

/**
 * A structure describing the private information for a dom0 device.
 */
struct dom0_mm_dev {
	struct miscdevice miscdev;
	uint8_t fail_times;
	uint32_t used_memsize;
	uint32_t num_mem_ctx;
	uint32_t config_memsize;
	uint32_t num_bigblock;
	struct  dom0_mm_data *mm_data[NUM_MEM_CTX];
	struct mutex data_lock;
};

struct dom0_mm_data{
	uint32_t refcnt;
	uint32_t num_memseg; /**< Number of memory segment. */
	uint32_t mem_size;   /**< Size of requesting memory. */

	char name[DOM0_NAME_MAX];

	/** Store global memory block IDs used by an instance */
	uint32_t block_num[DOM0_NUM_MEMBLOCK];

	/** Store memory block information.*/
	struct memblock_info block_info[DOM0_NUM_MEMBLOCK];

	/** Store memory segment information.*/
	struct memseg_info  seg_info[DOM0_NUM_MEMSEG];
};

#define XEN_ERR(args...) printk(KERN_DEBUG "XEN_DOM0: Error: " args)
#define XEN_PRINT(args...) printk(KERN_DEBUG "XEN_DOM0: " args)
#endif
