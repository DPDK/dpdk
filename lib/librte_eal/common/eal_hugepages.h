/*-
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
 */

#ifndef EAL_HUGEPAGES_H
#define EAL_HUGEPAGES_H

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#define MAX_HUGEPAGE_PATH PATH_MAX

/**
 * Structure used to store informations about hugepages that we mapped
 * through the files in hugetlbfs.
 */
struct hugepage_file {
	void *orig_va;      /**< virtual addr of first mmap() */
	void *final_va;     /**< virtual addr of 2nd mmap() */
	uint64_t physaddr;  /**< physical addr */
	size_t size;        /**< the page size */
	int socket_id;      /**< NUMA socket ID */
	int file_id;        /**< the '%d' in HUGEFILE_FMT */
	int memseg_id;      /**< the memory segment to which page belongs */
#ifdef RTE_EAL_SINGLE_FILE_SEGMENTS
	int repeated;		/**< number of times the page size is repeated */
#endif
	char filepath[MAX_HUGEPAGE_PATH]; /**< path to backing file on filesystem */
};

/**
 * Read the information from linux on what hugepages are available
 * for the EAL to use
 */
int eal_hugepage_info_init(void);

#endif /* EAL_HUGEPAGES_H */
