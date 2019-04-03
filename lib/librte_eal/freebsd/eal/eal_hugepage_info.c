/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
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
map_shared_memory(const char *filename, const size_t mem_size, int flags)
{
	void *retval;
	int fd = open(filename, flags, 0600);
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

static void *
open_shared_memory(const char *filename, const size_t mem_size)
{
	return map_shared_memory(filename, mem_size, O_RDWR);
}

static void *
create_shared_memory(const char *filename, const size_t mem_size)
{
	return map_shared_memory(filename, mem_size, O_RDWR | O_CREAT);
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
	unsigned int i;

	internal_config.num_hugepage_sizes = 1;

	sysctl_size = sizeof(num_buffers);
	error = sysctlbyname("hw.contigmem.num_buffers", &num_buffers,
			&sysctl_size, NULL, 0);

	if (error != 0) {
		RTE_LOG(ERR, EAL, "could not read sysctl hw.contigmem.num_buffers\n");
		return -1;
	}

	sysctl_size = sizeof(buffer_size);
	error = sysctlbyname("hw.contigmem.buffer_size", &buffer_size,
			&sysctl_size, NULL, 0);

	if (error != 0) {
		RTE_LOG(ERR, EAL, "could not read sysctl hw.contigmem.buffer_size\n");
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

	strlcpy(hpi->hugedir, CONTIGMEM_DEV, sizeof(hpi->hugedir));
	hpi->hugepage_sz = buffer_size;
	hpi->num_pages[0] = num_buffers;
	hpi->lock_descriptor = fd;

	/* for no shared files mode, do not create shared memory config */
	if (internal_config.no_shconf)
		return 0;

	tmp_hpi = create_shared_memory(eal_hugepage_info_path(),
			sizeof(internal_config.hugepage_info));
	if (tmp_hpi == NULL ) {
		RTE_LOG(ERR, EAL, "Failed to create shared memory!\n");
		return -1;
	}

	memcpy(tmp_hpi, hpi, sizeof(internal_config.hugepage_info));

	/* we've copied file descriptors along with everything else, but they
	 * will be invalid in secondary process, so overwrite them
	 */
	for (i = 0; i < RTE_DIM(internal_config.hugepage_info); i++) {
		struct hugepage_info *tmp = &tmp_hpi[i];
		tmp->lock_descriptor = -1;
	}

	if (munmap(tmp_hpi, sizeof(internal_config.hugepage_info)) < 0) {
		RTE_LOG(ERR, EAL, "Failed to unmap shared memory!\n");
		return -1;
	}

	return 0;
}

/* copy stuff from shared info into internal config */
int
eal_hugepage_info_read(void)
{
	struct hugepage_info *hpi = &internal_config.hugepage_info[0];
	struct hugepage_info *tmp_hpi;

	internal_config.num_hugepage_sizes = 1;

	tmp_hpi = open_shared_memory(eal_hugepage_info_path(),
				  sizeof(internal_config.hugepage_info));
	if (tmp_hpi == NULL) {
		RTE_LOG(ERR, EAL, "Failed to open shared memory!\n");
		return -1;
	}

	memcpy(hpi, tmp_hpi, sizeof(internal_config.hugepage_info));

	if (munmap(tmp_hpi, sizeof(internal_config.hugepage_info)) < 0) {
		RTE_LOG(ERR, EAL, "Failed to unmap shared memory!\n");
		return -1;
	}
	return 0;
}
