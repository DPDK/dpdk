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

#include <stdint.h>
#include <dirent.h>
#include <linux/vhost.h>
#include <linux/virtio_net.h>
#include <fuse/cuse_lowlevel.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include <rte_log.h>

#include "vhost-net.h"

/* Line size for reading maps file. */
static const uint32_t BUFSIZE = PATH_MAX;

/* Size of prot char array in procmap. */
#define PROT_SZ 5

/* Number of elements in procmap struct. */
#define PROCMAP_SZ 8

/* Structure containing information gathered from maps file. */
struct procmap {
	uint64_t va_start;	/* Start virtual address in file. */
	uint64_t len;		/* Size of file. */
	uint64_t pgoff;		/* Not used. */
	uint32_t maj;		/* Not used. */
	uint32_t min;		/* Not used. */
	uint32_t ino;		/* Not used. */
	char prot[PROT_SZ];	/* Not used. */
	char fname[PATH_MAX];	/* File name. */
};

/*
 * Locate the file containing QEMU's memory space and
 * map it to our address space.
 */
static int
host_memory_map(pid_t pid, uint64_t addr,
	uint64_t *mapped_address, uint64_t *mapped_size)
{
	struct dirent *dptr = NULL;
	struct procmap procmap;
	DIR *dp = NULL;
	int fd;
	int i;
	char memfile[PATH_MAX];
	char mapfile[PATH_MAX];
	char procdir[PATH_MAX];
	char resolved_path[PATH_MAX];
	char *path = NULL;
	FILE *fmap;
	void *map;
	uint8_t found = 0;
	char line[BUFSIZE];
	char dlm[] = "-   :   ";
	char *str, *sp, *in[PROCMAP_SZ];
	char *end = NULL;

	/* Path where mem files are located. */
	snprintf(procdir, PATH_MAX, "/proc/%u/fd/", pid);
	/* Maps file used to locate mem file. */
	snprintf(mapfile, PATH_MAX, "/proc/%u/maps", pid);

	fmap = fopen(mapfile, "r");
	if (fmap == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Failed to open maps file for pid %d\n",
			pid);
		return -1;
	}

	/* Read through maps file until we find out base_address. */
	while (fgets(line, BUFSIZE, fmap) != 0) {
		str = line;
		errno = 0;
		/* Split line into fields. */
		for (i = 0; i < PROCMAP_SZ; i++) {
			in[i] = strtok_r(str, &dlm[i], &sp);
			if ((in[i] == NULL) || (errno != 0)) {
				fclose(fmap);
				return -1;
			}
			str = NULL;
		}

		/* Convert/Copy each field as needed. */
		procmap.va_start = strtoull(in[0], &end, 16);
		if ((in[0] == '\0') || (end == NULL) || (*end != '\0') ||
			(errno != 0)) {
			fclose(fmap);
			return -1;
		}

		procmap.len = strtoull(in[1], &end, 16);
		if ((in[1] == '\0') || (end == NULL) || (*end != '\0') ||
			(errno != 0)) {
			fclose(fmap);
			return -1;
		}

		procmap.pgoff = strtoull(in[3], &end, 16);
		if ((in[3] == '\0') || (end == NULL) || (*end != '\0') ||
			(errno != 0)) {
			fclose(fmap);
			return -1;
		}

		procmap.maj = strtoul(in[4], &end, 16);
		if ((in[4] == '\0') || (end == NULL) || (*end != '\0') ||
			(errno != 0)) {
			fclose(fmap);
			return -1;
		}

		procmap.min = strtoul(in[5], &end, 16);
		if ((in[5] == '\0') || (end == NULL) || (*end != '\0') ||
			(errno != 0)) {
			fclose(fmap);
			return -1;
		}

		procmap.ino = strtoul(in[6], &end, 16);
		if ((in[6] == '\0') || (end == NULL) || (*end != '\0') ||
			(errno != 0)) {
			fclose(fmap);
			return -1;
		}

		memcpy(&procmap.prot, in[2], PROT_SZ);
		memcpy(&procmap.fname, in[7], PATH_MAX);

		if (procmap.va_start == addr) {
			procmap.len = procmap.len - procmap.va_start;
			found = 1;
			break;
		}
	}
	fclose(fmap);

	if (!found) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Failed to find memory file in pid %d maps file\n",
			pid);
		return -1;
	}

	/* Find the guest memory file among the process fds. */
	dp = opendir(procdir);
	if (dp == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Cannot open pid %d process directory\n",
			pid);
		return -1;
	}

	found = 0;

	/* Read the fd directory contents. */
	while (NULL != (dptr = readdir(dp))) {
		snprintf(memfile, PATH_MAX, "/proc/%u/fd/%s",
				pid, dptr->d_name);
		path = realpath(memfile, resolved_path);
		if ((path == NULL) && (strlen(resolved_path) == 0)) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"Failed to resolve fd directory\n");
			closedir(dp);
			return -1;
		}
		if (strncmp(resolved_path, procmap.fname,
			strnlen(procmap.fname, PATH_MAX)) == 0) {
			found = 1;
			break;
		}
	}

	closedir(dp);

	if (found == 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Failed to find memory file for pid %d\n",
			pid);
		return -1;
	}
	/* Open the shared memory file and map the memory into this process. */
	fd = open(memfile, O_RDWR);

	if (fd == -1) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Failed to open %s for pid %d\n",
			memfile, pid);
		return -1;
	}

	map = mmap(0, (size_t)procmap.len, PROT_READ|PROT_WRITE,
			MAP_POPULATE|MAP_SHARED, fd, 0);
	close(fd);

	if (map == MAP_FAILED) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Error mapping the file %s for pid %d\n",
			memfile, pid);
		return -1;
	}

	/* Store the memory address and size in the device data structure */
	*mapped_address = (uint64_t)(uintptr_t)map;
	*mapped_size = procmap.len;

	LOG_DEBUG(VHOST_CONFIG,
		"Mem File: %s->%s - Size: %llu - VA: %p\n",
		memfile, resolved_path,
		(unsigned long long)*mapped_size, map);

	return 0;
}
