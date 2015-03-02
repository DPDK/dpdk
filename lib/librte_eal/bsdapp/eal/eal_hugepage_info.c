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
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/mman.h>
#include <string.h>

#include <rte_log.h>
#include <fcntl.h>
#include "eal_hugepages.h"
#include "eal_internal_cfg.h"
#include "eal_filesystem.h"

#define CONTIGMEM_DEV "/dev/contigmem"

/*
 * Uses mmap to create a shared memory area for storage of data
 * Used in this file to store the hugepage file map on disk
 */
static void *
create_shared_memory(const char *filename, const size_t mem_size)
{
	void *retval;
	int fd = open(filename, O_CREAT | O_RDWR, 0666);
	if (fd < 0)
		return NULL;
	if (ftruncate(fd, mem_size) < 0) {
		close(fd);
		return NULL;
	}
	retval = mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	return retval;
}

/*
 * No hugepage support on freebsd, but we dummy it, using contigmem driver
 */
int
eal_hugepage_info_init(void)
{
	size_t sysctl_size;
	int num_buffers, fd, error;
	int64_t buffer_size;
	/* re-use the linux "internal config" structure for our memory data */
	struct hugepage_info *hpi = &internal_config.hugepage_info[0];
	struct hugepage_info *tmp_hpi;

	sysctl_size = sizeof(num_buffers);
	error = sysctlbyname("hw.contigmem.num_buffers", &num_buffers,
			&sysctl_size, NULL, 0);

	if (error != 0) {
		RTE_LOG(ERR, EAL, "could not read sysctl hw.contigmem.num_buffers");
		return -1;
	}

	sysctl_size = sizeof(buffer_size);
	error = sysctlbyname("hw.contigmem.buffer_size", &buffer_size,
			&sysctl_size, NULL, 0);

	if (error != 0) {
		RTE_LOG(ERR, EAL, "could not read sysctl hw.contigmem.buffer_size");
		return -1;
	}

	fd = open(CONTIGMEM_DEV, O_RDWR);
	if (fd < 0) {
		RTE_LOG(ERR, EAL, "could not open "CONTIGMEM_DEV"\n");
		return -1;
	}

	if (buffer_size >= 1<<30)
		RTE_LOG(INFO, EAL, "Contigmem driver has %d buffers, each of size %dGB\n",
				num_buffers, (int)(buffer_size>>30));
	else if (buffer_size >= 1<<20)
		RTE_LOG(INFO, EAL, "Contigmem driver has %d buffers, each of size %dMB\n",
				num_buffers, (int)(buffer_size>>20));
	else
		RTE_LOG(INFO, EAL, "Contigmem driver has %d buffers, each of size %dKB\n",
				num_buffers, (int)(buffer_size>>10));

	internal_config.num_hugepage_sizes = 1;
	hpi->hugedir = CONTIGMEM_DEV;
	hpi->hugepage_sz = buffer_size;
	hpi->num_pages[0] = num_buffers;
	hpi->lock_descriptor = fd;

	tmp_hpi = create_shared_memory(eal_hugepage_info_path(),
					sizeof(struct hugepage_info));
	if (tmp_hpi == NULL ) {
		RTE_LOG(ERR, EAL, "Failed to create shared memory!\n");
		return -1;
	}

	memcpy(tmp_hpi, hpi, sizeof(struct hugepage_info));

	if ( munmap(tmp_hpi, sizeof(struct hugepage_info)) < 0) {
		RTE_LOG(ERR, EAL, "Failed to unmap shared memory!\n");
		return -1;
	}

	return 0;
}
