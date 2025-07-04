/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Dmitry Kozlyuk
 */

#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <inttypes.h>

#include <rte_eal_paging.h>
#include <rte_errno.h>
#include <rte_log.h>

#include <eal_export.h>
#include "eal_private.h"

#ifdef RTE_EXEC_ENV_LINUX
#define EAL_DONTDUMP MADV_DONTDUMP
#define EAL_DODUMP   MADV_DODUMP
#define EAL_FIXED_NOREPLACE MAP_FIXED_NOREPLACE
#elif defined RTE_EXEC_ENV_FREEBSD
#define EAL_DONTDUMP MADV_NOCORE
#define EAL_DODUMP   MADV_CORE
#define EAL_FIXED_NOREPLACE (MAP_FIXED | MAP_EXCL)
#else
#error "EAL doesn't support this OS"
#endif

static void *
mem_map(void *requested_addr, size_t size, int prot, int flags,
	int fd, uint64_t offset)
{
	void *virt = mmap(requested_addr, size, prot, flags, fd, offset);
	if (virt == MAP_FAILED) {
		EAL_LOG(DEBUG,
		    "Cannot mmap(%p, 0x%zx, 0x%x, 0x%x, %d, 0x%"PRIx64"): %s",
		    requested_addr, size, prot, flags, fd, offset,
		    strerror(errno));
		rte_errno = errno;
		return NULL;
	}
	return virt;
}

static int
mem_unmap(void *virt, size_t size)
{
	int ret = munmap(virt, size);
	if (ret < 0) {
		EAL_LOG(DEBUG, "Cannot munmap(%p, 0x%zx): %s",
			virt, size, strerror(errno));
		rte_errno = errno;
	}
	return ret;
}

void *
eal_mem_reserve(void *requested_addr, size_t size, int flags)
{
	int sys_flags = MAP_PRIVATE | MAP_ANONYMOUS;

	if (flags & EAL_RESERVE_HUGEPAGES) {
#ifdef MAP_HUGETLB
		sys_flags |= MAP_HUGETLB;
#else
		rte_errno = ENOTSUP;
		return NULL;
#endif
	}

	if (flags & EAL_RESERVE_FORCE_ADDRESS)
		sys_flags |= MAP_FIXED;
	if (flags & EAL_RESERVE_FORCE_ADDRESS_NOREPLACE)
		sys_flags |= EAL_FIXED_NOREPLACE;

	return mem_map(requested_addr, size, PROT_NONE, sys_flags, -1, 0);
}

void
eal_mem_free(void *virt, size_t size)
{
	mem_unmap(virt, size);
}

int
eal_mem_set_dump(void *virt, size_t size, bool dump)
{
	int flags = dump ? EAL_DODUMP : EAL_DONTDUMP;
	int ret = madvise(virt, size, flags);
	if (ret) {
		EAL_LOG(DEBUG, "madvise(%p, %#zx, %d) failed: %s",
				virt, size, flags, strerror(errno));
		rte_errno = errno;
	}
	return ret;
}

static int
mem_rte_to_sys_prot(int prot)
{
	int sys_prot = PROT_NONE;

	if (prot & RTE_PROT_READ)
		sys_prot |= PROT_READ;
	if (prot & RTE_PROT_WRITE)
		sys_prot |= PROT_WRITE;
	if (prot & RTE_PROT_EXECUTE)
		sys_prot |= PROT_EXEC;

	return sys_prot;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_mem_map)
void *
rte_mem_map(void *requested_addr, size_t size, int prot, int flags,
	int fd, uint64_t offset)
{
	int sys_flags = 0;
	int sys_prot;

	sys_prot = mem_rte_to_sys_prot(prot);

	if (flags & RTE_MAP_SHARED)
		sys_flags |= MAP_SHARED;
	if (flags & RTE_MAP_ANONYMOUS)
		sys_flags |= MAP_ANONYMOUS;
	if (flags & RTE_MAP_PRIVATE)
		sys_flags |= MAP_PRIVATE;
	if (flags & RTE_MAP_FORCE_ADDRESS)
		sys_flags |= MAP_FIXED;
	if (flags & RTE_MAP_FORCE_ADDRESS_NOREPLACE)
		sys_flags |= EAL_FIXED_NOREPLACE;

	return mem_map(requested_addr, size, sys_prot, sys_flags, fd, offset);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_mem_unmap)
int
rte_mem_unmap(void *virt, size_t size)
{
	return mem_unmap(virt, size);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_mem_page_size)
size_t
rte_mem_page_size(void)
{
	static size_t page_size;

	if (unlikely(page_size == 0)) {
		/*
		 * When the sysconf value cannot be determined, sysconf()
		 * returns -1 without setting errno.
		 * To distinguish an indeterminate value from an error,
		 * clear errno before calling sysconf(), and check whether
		 * errno has been set if sysconf() returns -1.
		 */
		errno = 0;
		page_size = sysconf(_SC_PAGESIZE);
		if ((ssize_t)page_size < 0)
			rte_panic("sysconf(_SC_PAGESIZE) failed: %s",
					errno == 0 ? "Indeterminate" : strerror(errno));
	}

	return page_size;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_mem_lock)
int
rte_mem_lock(const void *virt, size_t size)
{
	int ret = mlock(virt, size);
	if (ret)
		rte_errno = errno;
	return ret;
}
