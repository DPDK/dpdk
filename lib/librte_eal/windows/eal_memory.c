/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Dmitry Kozlyuk
 */

#include <inttypes.h>
#include <io.h>

#include <rte_eal_paging.h>
#include <rte_errno.h>

#include "eal_internal_cfg.h"
#include "eal_memalloc.h"
#include "eal_memcfg.h"
#include "eal_options.h"
#include "eal_private.h"
#include "eal_windows.h"

#include <rte_virt2phys.h>

/* MinGW-w64 headers lack VirtualAlloc2() in some distributions.
 * Note: definitions are copied verbatim from Microsoft documentation
 * and don't follow DPDK code style.
 */
#ifndef MEM_EXTENDED_PARAMETER_TYPE_BITS

#define MEM_EXTENDED_PARAMETER_TYPE_BITS 4

/* https://docs.microsoft.com/en-us/windows/win32/api/winnt/ne-winnt-mem_extended_parameter_type */
typedef enum MEM_EXTENDED_PARAMETER_TYPE {
	MemExtendedParameterInvalidType,
	MemExtendedParameterAddressRequirements,
	MemExtendedParameterNumaNode,
	MemExtendedParameterPartitionHandle,
	MemExtendedParameterUserPhysicalHandle,
	MemExtendedParameterAttributeFlags,
	MemExtendedParameterMax
} *PMEM_EXTENDED_PARAMETER_TYPE;

/* https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-mem_extended_parameter */
typedef struct MEM_EXTENDED_PARAMETER {
	struct {
		DWORD64 Type : MEM_EXTENDED_PARAMETER_TYPE_BITS;
		DWORD64 Reserved : 64 - MEM_EXTENDED_PARAMETER_TYPE_BITS;
	} DUMMYSTRUCTNAME;
	union {
		DWORD64 ULong64;
		PVOID   Pointer;
		SIZE_T  Size;
		HANDLE  Handle;
		DWORD   ULong;
	} DUMMYUNIONNAME;
} MEM_EXTENDED_PARAMETER, *PMEM_EXTENDED_PARAMETER;

#endif /* defined(MEM_EXTENDED_PARAMETER_TYPE_BITS) */

/* https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2 */
typedef PVOID (*VirtualAlloc2_type)(
	HANDLE                 Process,
	PVOID                  BaseAddress,
	SIZE_T                 Size,
	ULONG                  AllocationType,
	ULONG                  PageProtection,
	MEM_EXTENDED_PARAMETER *ExtendedParameters,
	ULONG                  ParameterCount
);

/* MinGW-w64 distributions, even those that declare VirtualAlloc2(),
 * lack it in import libraries, which results in a failure at link time.
 * Link it dynamically in such case.
 */
static VirtualAlloc2_type VirtualAlloc2_ptr;

#ifdef RTE_TOOLCHAIN_GCC

#define MEM_COALESCE_PLACEHOLDERS 0x00000001
#define MEM_PRESERVE_PLACEHOLDER  0x00000002
#define MEM_REPLACE_PLACEHOLDER   0x00004000
#define MEM_RESERVE_PLACEHOLDER   0x00040000

int
eal_mem_win32api_init(void)
{
	/* Contrary to the docs, VirtualAlloc2() is not in kernel32.dll,
	 * see https://github.com/MicrosoftDocs/feedback/issues/1129.
	 */
	static const char library_name[] = "kernelbase.dll";
	static const char function[] = "VirtualAlloc2";

	HMODULE library = NULL;
	int ret = 0;

	/* Already done. */
	if (VirtualAlloc2_ptr != NULL)
		return 0;

	library = LoadLibraryA(library_name);
	if (library == NULL) {
		RTE_LOG_WIN32_ERR("LoadLibraryA(\"%s\")", library_name);
		return -1;
	}

	VirtualAlloc2_ptr = (VirtualAlloc2_type)(
		(void *)GetProcAddress(library, function));
	if (VirtualAlloc2_ptr == NULL) {
		RTE_LOG_WIN32_ERR("GetProcAddress(\"%s\", \"%s\")\n",
			library_name, function);

		/* Contrary to the docs, Server 2016 is not supported. */
		RTE_LOG(ERR, EAL, "Windows 10 or Windows Server 2019 "
			" is required for memory management\n");
		ret = -1;
	}

	FreeLibrary(library);

	return ret;
}

#else

/* Stub in case VirtualAlloc2() is provided by the toolchain. */
int
eal_mem_win32api_init(void)
{
	VirtualAlloc2_ptr = VirtualAlloc2;
	return 0;
}

#endif /* defined(RTE_TOOLCHAIN_GCC) */

static HANDLE virt2phys_device = INVALID_HANDLE_VALUE;

int
eal_mem_virt2iova_init(void)
{
	HDEVINFO list = INVALID_HANDLE_VALUE;
	SP_DEVICE_INTERFACE_DATA ifdata;
	SP_DEVICE_INTERFACE_DETAIL_DATA *detail = NULL;
	DWORD detail_size;
	int ret = -1;

	list = SetupDiGetClassDevs(
		&GUID_DEVINTERFACE_VIRT2PHYS, NULL, NULL,
		DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
	if (list == INVALID_HANDLE_VALUE) {
		RTE_LOG_WIN32_ERR("SetupDiGetClassDevs()");
		goto exit;
	}

	ifdata.cbSize = sizeof(ifdata);
	if (!SetupDiEnumDeviceInterfaces(
		list, NULL, &GUID_DEVINTERFACE_VIRT2PHYS, 0, &ifdata)) {
		RTE_LOG_WIN32_ERR("SetupDiEnumDeviceInterfaces()");
		goto exit;
	}

	if (!SetupDiGetDeviceInterfaceDetail(
		list, &ifdata, NULL, 0, &detail_size, NULL)) {
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			RTE_LOG_WIN32_ERR(
				"SetupDiGetDeviceInterfaceDetail(probe)");
			goto exit;
		}
	}

	detail = malloc(detail_size);
	if (detail == NULL) {
		RTE_LOG(ERR, EAL, "Cannot allocate virt2phys "
			"device interface detail data\n");
		goto exit;
	}

	detail->cbSize = sizeof(*detail);
	if (!SetupDiGetDeviceInterfaceDetail(
		list, &ifdata, detail, detail_size, NULL, NULL)) {
		RTE_LOG_WIN32_ERR("SetupDiGetDeviceInterfaceDetail(read)");
		goto exit;
	}

	RTE_LOG(DEBUG, EAL, "Found virt2phys device: %s\n", detail->DevicePath);

	virt2phys_device = CreateFile(
		detail->DevicePath, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (virt2phys_device == INVALID_HANDLE_VALUE) {
		RTE_LOG_WIN32_ERR("CreateFile()");
		goto exit;
	}

	/* Indicate success. */
	ret = 0;

exit:
	if (detail != NULL)
		free(detail);
	if (list != INVALID_HANDLE_VALUE)
		SetupDiDestroyDeviceInfoList(list);

	return ret;
}

void
eal_mem_virt2iova_cleanup(void)
{
	if (virt2phys_device != INVALID_HANDLE_VALUE)
		CloseHandle(virt2phys_device);
}

phys_addr_t
rte_mem_virt2phy(const void *virt)
{
	LARGE_INTEGER phys;
	DWORD bytes_returned;

	if (virt2phys_device == INVALID_HANDLE_VALUE)
		return RTE_BAD_PHYS_ADDR;

	if (!DeviceIoControl(
			virt2phys_device, IOCTL_VIRT2PHYS_TRANSLATE,
			&virt, sizeof(virt), &phys, sizeof(phys),
			&bytes_returned, NULL)) {
		RTE_LOG_WIN32_ERR("DeviceIoControl(IOCTL_VIRT2PHYS_TRANSLATE)");
		return RTE_BAD_PHYS_ADDR;
	}

	return phys.QuadPart;
}

/* Windows currently only supports IOVA as PA. */
rte_iova_t
rte_mem_virt2iova(const void *virt)
{
	phys_addr_t phys;

	if (virt2phys_device == INVALID_HANDLE_VALUE)
		return RTE_BAD_IOVA;

	phys = rte_mem_virt2phy(virt);
	if (phys == RTE_BAD_PHYS_ADDR)
		return RTE_BAD_IOVA;

	return (rte_iova_t)phys;
}

/* Always using physical addresses under Windows if they can be obtained. */
int
rte_eal_using_phys_addrs(void)
{
	return virt2phys_device != INVALID_HANDLE_VALUE;
}

/* Approximate error mapping from VirtualAlloc2() to POSIX mmap(3). */
static void
set_errno_from_win32_alloc_error(DWORD code)
{
	switch (code) {
	case ERROR_SUCCESS:
		rte_errno = 0;
		break;

	case ERROR_INVALID_ADDRESS:
		/* A valid requested address is not available. */
	case ERROR_COMMITMENT_LIMIT:
		/* May occur when committing regular memory. */
	case ERROR_NO_SYSTEM_RESOURCES:
		/* Occurs when the system runs out of hugepages. */
		rte_errno = ENOMEM;
		break;

	case ERROR_INVALID_PARAMETER:
	default:
		rte_errno = EINVAL;
		break;
	}
}

void *
eal_mem_reserve(void *requested_addr, size_t size, int flags)
{
	HANDLE process;
	void *virt;

	/* Windows requires hugepages to be committed. */
	if (flags & EAL_RESERVE_HUGEPAGES) {
		rte_errno = ENOTSUP;
		return NULL;
	}

	process = GetCurrentProcess();

	virt = VirtualAlloc2_ptr(process, requested_addr, size,
		MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS,
		NULL, 0);
	if (virt == NULL) {
		DWORD err = GetLastError();
		RTE_LOG_WIN32_ERR("VirtualAlloc2()");
		set_errno_from_win32_alloc_error(err);
		return NULL;
	}

	if ((flags & EAL_RESERVE_FORCE_ADDRESS) && (virt != requested_addr)) {
		if (!VirtualFreeEx(process, virt, 0, MEM_RELEASE))
			RTE_LOG_WIN32_ERR("VirtualFreeEx()");
		rte_errno = ENOMEM;
		return NULL;
	}

	return virt;
}

void *
eal_mem_alloc_socket(size_t size, int socket_id)
{
	DWORD flags = MEM_RESERVE | MEM_COMMIT;
	void *addr;

	flags = MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES;
	addr = VirtualAllocExNuma(GetCurrentProcess(), NULL, size, flags,
		PAGE_READWRITE, eal_socket_numa_node(socket_id));
	if (addr == NULL)
		rte_errno = ENOMEM;
	return addr;
}

void *
eal_mem_commit(void *requested_addr, size_t size, int socket_id)
{
	HANDLE process;
	MEM_EXTENDED_PARAMETER param;
	DWORD param_count = 0;
	DWORD flags;
	void *addr;

	process = GetCurrentProcess();

	if (requested_addr != NULL) {
		MEMORY_BASIC_INFORMATION info;

		if (VirtualQueryEx(process, requested_addr, &info,
				sizeof(info)) != sizeof(info)) {
			RTE_LOG_WIN32_ERR("VirtualQuery(%p)", requested_addr);
			return NULL;
		}

		/* Split reserved region if only a part is committed. */
		flags = MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER;
		if ((info.RegionSize > size) && !VirtualFreeEx(
				process, requested_addr, size, flags)) {
			RTE_LOG_WIN32_ERR(
				"VirtualFreeEx(%p, %zu, preserve placeholder)",
				requested_addr, size);
			return NULL;
		}

		/* Temporarily release the region to be committed.
		 *
		 * There is an inherent race for this memory range
		 * if another thread allocates memory via OS API.
		 * However, VirtualAlloc2(MEM_REPLACE_PLACEHOLDER)
		 * doesn't work with MEM_LARGE_PAGES on Windows Server.
		 */
		if (!VirtualFreeEx(process, requested_addr, 0, MEM_RELEASE)) {
			RTE_LOG_WIN32_ERR("VirtualFreeEx(%p, 0, release)",
				requested_addr);
			return NULL;
		}
	}

	if (socket_id != SOCKET_ID_ANY) {
		param_count = 1;
		memset(&param, 0, sizeof(param));
		param.Type = MemExtendedParameterNumaNode;
		param.ULong = eal_socket_numa_node(socket_id);
	}

	flags = MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES;
	addr = VirtualAlloc2_ptr(process, requested_addr, size,
		flags, PAGE_READWRITE, &param, param_count);
	if (addr == NULL) {
		/* Logging may overwrite GetLastError() result. */
		DWORD err = GetLastError();
		RTE_LOG_WIN32_ERR("VirtualAlloc2(%p, %zu, commit large pages)",
			requested_addr, size);
		set_errno_from_win32_alloc_error(err);
		return NULL;
	}

	if ((requested_addr != NULL) && (addr != requested_addr)) {
		/* We lost the race for the requested_addr. */
		if (!VirtualFreeEx(process, addr, 0, MEM_RELEASE))
			RTE_LOG_WIN32_ERR("VirtualFreeEx(%p, release)", addr);

		rte_errno = EADDRNOTAVAIL;
		return NULL;
	}

	return addr;
}

int
eal_mem_decommit(void *addr, size_t size)
{
	HANDLE process;
	void *stub;
	DWORD flags;

	process = GetCurrentProcess();

	/* Hugepages cannot be decommited on Windows,
	 * so free them and replace the block with a placeholder.
	 * There is a race for VA in this block until VirtualAlloc2 call.
	 */
	if (!VirtualFreeEx(process, addr, 0, MEM_RELEASE)) {
		RTE_LOG_WIN32_ERR("VirtualFreeEx(%p, 0, release)", addr);
		return -1;
	}

	flags = MEM_RESERVE | MEM_RESERVE_PLACEHOLDER;
	stub = VirtualAlloc2_ptr(
		process, addr, size, flags, PAGE_NOACCESS, NULL, 0);
	if (stub == NULL) {
		/* We lost the race for the VA. */
		if (!VirtualFreeEx(process, stub, 0, MEM_RELEASE))
			RTE_LOG_WIN32_ERR("VirtualFreeEx(%p, release)", stub);
		rte_errno = EADDRNOTAVAIL;
		return -1;
	}

	/* No need to join reserved regions adjacent to the freed one:
	 * eal_mem_commit() will just pick up the page-size placeholder
	 * created here.
	 */
	return 0;
}

/**
 * Free a reserved memory region in full or in part.
 *
 * @param addr
 *  Starting address of the area to free.
 * @param size
 *  Number of bytes to free. Must be a multiple of page size.
 * @param reserved
 *  Fail if the region is not in reserved state.
 * @return
 *  * 0 on successful deallocation;
 *  * 1 if region must be in reserved state but it is not;
 *  * (-1) on system API failures.
 */
static int
mem_free(void *addr, size_t size, bool reserved)
{
	MEMORY_BASIC_INFORMATION info;
	HANDLE process;

	process = GetCurrentProcess();

	if (VirtualQueryEx(
			process, addr, &info, sizeof(info)) != sizeof(info)) {
		RTE_LOG_WIN32_ERR("VirtualQueryEx(%p)", addr);
		return -1;
	}

	if (reserved && (info.State != MEM_RESERVE))
		return 1;

	/* Free complete region. */
	if ((addr == info.AllocationBase) && (size == info.RegionSize)) {
		if (!VirtualFreeEx(process, addr, 0, MEM_RELEASE)) {
			RTE_LOG_WIN32_ERR("VirtualFreeEx(%p, 0, release)",
				addr);
		}
		return 0;
	}

	/* Split the part to be freed and the remaining reservation. */
	if (!VirtualFreeEx(process, addr, size,
			MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)) {
		RTE_LOG_WIN32_ERR(
			"VirtualFreeEx(%p, %zu, preserve placeholder)",
			addr, size);
		return -1;
	}

	/* Actually free reservation part. */
	if (!VirtualFreeEx(process, addr, 0, MEM_RELEASE)) {
		RTE_LOG_WIN32_ERR("VirtualFreeEx(%p, 0, release)", addr);
		return -1;
	}

	return 0;
}

void
eal_mem_free(void *virt, size_t size)
{
	mem_free(virt, size, false);
}

int
eal_mem_set_dump(void *virt, size_t size, bool dump)
{
	RTE_SET_USED(virt);
	RTE_SET_USED(size);
	RTE_SET_USED(dump);

	/* Windows does not dump reserved memory by default.
	 *
	 * There is <werapi.h> to include or exclude regions from the dump,
	 * but this is not currently required by EAL.
	 */

	rte_errno = ENOTSUP;
	return -1;
}

void *
rte_mem_map(void *requested_addr, size_t size, int prot, int flags,
	int fd, uint64_t offset)
{
	HANDLE file_handle = INVALID_HANDLE_VALUE;
	HANDLE mapping_handle = INVALID_HANDLE_VALUE;
	DWORD sys_prot = 0;
	DWORD sys_access = 0;
	DWORD size_high = (DWORD)(size >> 32);
	DWORD size_low = (DWORD)size;
	DWORD offset_high = (DWORD)(offset >> 32);
	DWORD offset_low = (DWORD)offset;
	LPVOID virt = NULL;

	if (prot & RTE_PROT_EXECUTE) {
		if (prot & RTE_PROT_READ) {
			sys_prot = PAGE_EXECUTE_READ;
			sys_access = FILE_MAP_READ | FILE_MAP_EXECUTE;
		}
		if (prot & RTE_PROT_WRITE) {
			sys_prot = PAGE_EXECUTE_READWRITE;
			sys_access = FILE_MAP_WRITE | FILE_MAP_EXECUTE;
		}
	} else {
		if (prot & RTE_PROT_READ) {
			sys_prot = PAGE_READONLY;
			sys_access = FILE_MAP_READ;
		}
		if (prot & RTE_PROT_WRITE) {
			sys_prot = PAGE_READWRITE;
			sys_access = FILE_MAP_WRITE;
		}
	}

	if (flags & RTE_MAP_PRIVATE)
		sys_access |= FILE_MAP_COPY;

	if ((flags & RTE_MAP_ANONYMOUS) == 0)
		file_handle = (HANDLE)_get_osfhandle(fd);

	mapping_handle = CreateFileMapping(
		file_handle, NULL, sys_prot, size_high, size_low, NULL);
	if (mapping_handle == INVALID_HANDLE_VALUE) {
		RTE_LOG_WIN32_ERR("CreateFileMapping()");
		return NULL;
	}

	/* There is a race for the requested_addr between mem_free()
	 * and MapViewOfFileEx(). MapViewOfFile3() that can replace a reserved
	 * region with a mapping in a single operation, but it does not support
	 * private mappings.
	 */
	if (requested_addr != NULL) {
		int ret = mem_free(requested_addr, size, true);
		if (ret) {
			if (ret > 0) {
				RTE_LOG(ERR, EAL, "Cannot map memory "
					"to a region not reserved\n");
				rte_errno = EADDRNOTAVAIL;
			}
			return NULL;
		}
	}

	virt = MapViewOfFileEx(mapping_handle, sys_access,
		offset_high, offset_low, size, requested_addr);
	if (!virt) {
		RTE_LOG_WIN32_ERR("MapViewOfFileEx()");
		return NULL;
	}

	if ((flags & RTE_MAP_FORCE_ADDRESS) && (virt != requested_addr)) {
		if (!UnmapViewOfFile(virt))
			RTE_LOG_WIN32_ERR("UnmapViewOfFile()");
		virt = NULL;
	}

	if (!CloseHandle(mapping_handle))
		RTE_LOG_WIN32_ERR("CloseHandle()");

	return virt;
}

int
rte_mem_unmap(void *virt, size_t size)
{
	RTE_SET_USED(size);

	if (!UnmapViewOfFile(virt)) {
		RTE_LOG_WIN32_ERR("UnmapViewOfFile()");
		rte_errno = EINVAL;
		return -1;
	}
	return 0;
}

uint64_t
eal_get_baseaddr(void)
{
	/* Windows strategy for memory allocation is undocumented.
	 * Returning 0 here effectively disables address guessing
	 * unless user provides an address hint.
	 */
	return 0;
}

size_t
rte_mem_page_size(void)
{
	static SYSTEM_INFO info;

	if (info.dwPageSize == 0)
		GetSystemInfo(&info);

	return info.dwPageSize;
}

int
rte_mem_lock(const void *virt, size_t size)
{
	/* VirtualLock() takes `void*`, work around compiler warning. */
	void *addr = (void *)((uintptr_t)virt);

	if (!VirtualLock(addr, size)) {
		RTE_LOG_WIN32_ERR("VirtualLock(%p %#zx)", virt, size);
		return -1;
	}

	return 0;
}

int
rte_eal_memseg_init(void)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		EAL_LOG_NOT_IMPLEMENTED();
		return -1;
	}

	return eal_dynmem_memseg_lists_init();
}

static int
eal_nohuge_init(void)
{
	struct rte_mem_config *mcfg;
	struct rte_memseg_list *msl;
	int n_segs;
	uint64_t mem_sz, page_sz;
	void *addr;

	mcfg = rte_eal_get_configuration()->mem_config;
	struct internal_config *internal_conf =
		eal_get_internal_configuration();

	/* nohuge mode is legacy mode */
	internal_conf->legacy_mem = 1;

	msl = &mcfg->memsegs[0];

	mem_sz = internal_conf->memory;
	page_sz = RTE_PGSIZE_4K;
	n_segs = mem_sz / page_sz;

	if (eal_memseg_list_init_named(
			msl, "nohugemem", page_sz, n_segs, 0, true)) {
		return -1;
	}

	addr = VirtualAlloc(
		NULL, mem_sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (addr == NULL) {
		RTE_LOG_WIN32_ERR("VirtualAlloc(size=%#zx)", mem_sz);
		RTE_LOG(ERR, EAL, "Cannot allocate memory\n");
		return -1;
	}

	msl->base_va = addr;
	msl->len = mem_sz;

	eal_memseg_list_populate(msl, addr, n_segs);

	if (mcfg->dma_maskbits &&
		rte_mem_check_dma_mask_thread_unsafe(mcfg->dma_maskbits)) {
		RTE_LOG(ERR, EAL,
			"%s(): couldn't allocate memory due to IOVA "
			"exceeding limits of current DMA mask.\n", __func__);
		return -1;
	}

	return 0;
}

int
rte_eal_hugepage_init(void)
{
	const struct internal_config *internal_conf =
		eal_get_internal_configuration();

	return internal_conf->no_hugetlbfs ?
		eal_nohuge_init() : eal_dynmem_hugepage_init();
}

int
rte_eal_hugepage_attach(void)
{
	EAL_LOG_NOT_IMPLEMENTED();
	return -1;
}
