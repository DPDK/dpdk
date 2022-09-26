/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#include <dlfcn.h>

#include <rte_malloc.h>
#include <rte_pci.h>
#include <bus_pci_driver.h>
#include <rte_byteorder.h>
#include <dev_driver.h>

#include <gpudev_driver.h>

#include <cuda.h>
#include <cudaTypedefs.h>

#include "common.h"
#include "devices.h"

#define CUDA_DRIVER_MIN_VERSION 11040
#define CUDA_API_MIN_VERSION 3020

/* CUDA Driver functions loaded with dlsym() */
static CUresult CUDAAPI (*sym_cuInit)(unsigned int flags);
static CUresult CUDAAPI (*sym_cuDriverGetVersion)(int *driverVersion);
static CUresult CUDAAPI (*sym_cuGetProcAddress)(const char *symbol,
		void **pfn, int cudaVersion, uint64_t flags);

/* CUDA Driver functions loaded with cuGetProcAddress for versioning */
static PFN_cuGetErrorString pfn_cuGetErrorString;
static PFN_cuGetErrorName pfn_cuGetErrorName;
static PFN_cuPointerSetAttribute pfn_cuPointerSetAttribute;
static PFN_cuDeviceGetAttribute pfn_cuDeviceGetAttribute;
static PFN_cuDeviceGetByPCIBusId pfn_cuDeviceGetByPCIBusId;
static PFN_cuDevicePrimaryCtxRetain pfn_cuDevicePrimaryCtxRetain;
static PFN_cuDevicePrimaryCtxRelease pfn_cuDevicePrimaryCtxRelease;
static PFN_cuDeviceTotalMem pfn_cuDeviceTotalMem;
static PFN_cuDeviceGetName pfn_cuDeviceGetName;
static PFN_cuCtxGetApiVersion pfn_cuCtxGetApiVersion;
static PFN_cuCtxSetCurrent pfn_cuCtxSetCurrent;
static PFN_cuCtxGetCurrent pfn_cuCtxGetCurrent;
static PFN_cuCtxGetDevice pfn_cuCtxGetDevice;
static PFN_cuCtxGetExecAffinity pfn_cuCtxGetExecAffinity;
static PFN_cuMemAlloc pfn_cuMemAlloc;
static PFN_cuMemFree pfn_cuMemFree;
static PFN_cuMemHostRegister pfn_cuMemHostRegister;
static PFN_cuMemHostUnregister pfn_cuMemHostUnregister;
static PFN_cuMemHostGetDevicePointer pfn_cuMemHostGetDevicePointer;
static PFN_cuFlushGPUDirectRDMAWrites pfn_cuFlushGPUDirectRDMAWrites;

static void *cudalib;
static unsigned int cuda_api_version;
static int cuda_driver_version;
static gdr_t gdrc_h;

#define CUDA_MAX_ALLOCATION_NUM 512

#define GPU_PAGE_SHIFT 16
#define GPU_PAGE_SIZE (1UL << GPU_PAGE_SHIFT)

RTE_LOG_REGISTER_DEFAULT(cuda_logtype, NOTICE);

/* NVIDIA GPU address map */
static const struct rte_pci_id pci_id_cuda_map[] = {
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_A40_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_A30_24GB_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_A30X_24GB_DPU_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_A10_24GB_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_A10G_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_A10M_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_A100_40GB_SXM4_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_A100_40GB_PCIE_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_A100_80GB_SXM4_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_A100_80GB_PCIE_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_A100X_80GB_DPU_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_GA100_PG506_207)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_GA100_PCIE)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_GA100_PG506_217)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_V100_16GB_SXM2_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_V100_16GB_DGXS_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_V100_16GB_FHHL_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_V100_16GB_PCIE_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_V100_32GB_SXM2_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_V100_32GB_PCIE_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_V100_32GB_DGXS_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_V100_32GB_SXM3_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_V100_32GB_SXM3_H_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_V100_SXM2)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_V100S_PCIE)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_TITAN_V_CEO_ED)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_GV100GL_PG500_216)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_GV100GL_PG503_216)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_TU102_TITAN_RTX)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_TU102GL_QUADRO_RTX)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_GV100_QUADRO_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_QUADRO_RTX_4000)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_QUADRO_RTX_5000)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_QUADRO_RTX_6000)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_QUADRO_RTX_8000)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_QUADRO_RTX_A4000)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_QUADRO_RTX_A6000)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_QUADRO_RTX_A5000)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_QUADRO_RTX_A4500)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_QUADRO_RTX_A5500)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_QUADRO_RTX_A2000)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_QUADRO_RTX_A2000_12GB)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_T4G)
	},
	{
		RTE_PCI_DEVICE(NVIDIA_GPU_VENDOR_ID,
				NVIDIA_GPU_T4)
	},
	{
		.device_id = 0
	}
};

/* Device private info */
struct cuda_info {
	char gpu_name[RTE_DEV_NAME_MAX_LEN];
	CUdevice cu_dev;
	int gdr_supported;
	int gdr_write_ordering;
	int gdr_flush_type;
};

/* Type of memory allocated by CUDA driver */
enum mem_type {
	GPU_MEM = 0,
	CPU_REGISTERED,
	GPU_REGISTERED
};

/* key associated to a memory address */
typedef uintptr_t cuda_ptr_key;

/* Single entry of the memory list */
struct mem_entry {
	CUdeviceptr ptr_d;
	CUdeviceptr ptr_orig_d;
	void *ptr_h;
	size_t size;
	size_t size_orig;
	struct rte_gpu *dev;
	CUcontext ctx;
	cuda_ptr_key pkey;
	enum mem_type mtype;
	gdr_mh_t mh;
	struct mem_entry *prev;
	struct mem_entry *next;
};

static struct mem_entry *mem_alloc_list_head;
static struct mem_entry *mem_alloc_list_tail;
static uint32_t mem_alloc_list_last_elem;

/* Load the CUDA symbols */

static int
cuda_loader(void)
{
	char cuda_path[1024];

	if (getenv("CUDA_PATH_L") == NULL)
		snprintf(cuda_path, 1024, "%s", "libcuda.so");
	else
		snprintf(cuda_path, 1024, "%s/%s", getenv("CUDA_PATH_L"), "libcuda.so");

	cudalib = dlopen(cuda_path, RTLD_LAZY);
	if (cudalib == NULL) {
		rte_cuda_log(ERR, "Failed to find CUDA library in %s (CUDA_PATH_L=%s)",
				cuda_path, getenv("CUDA_PATH_L"));
		return -1;
	}

	return 0;
}

static int
cuda_sym_func_loader(void)
{
	if (cudalib == NULL)
		return -1;

	sym_cuInit = dlsym(cudalib, "cuInit");
	if (sym_cuInit == NULL) {
		rte_cuda_log(ERR, "Failed to load CUDA missing symbol cuInit");
		return -1;
	}

	sym_cuDriverGetVersion = dlsym(cudalib, "cuDriverGetVersion");
	if (sym_cuDriverGetVersion == NULL) {
		rte_cuda_log(ERR, "Failed to load CUDA missing symbol cuDriverGetVersion");
		return -1;
	}

	sym_cuGetProcAddress = dlsym(cudalib, "cuGetProcAddress");
	if (sym_cuGetProcAddress == NULL) {
		rte_cuda_log(ERR, "Failed to load CUDA missing symbol cuGetProcAddress");
		return -1;
	}

	return 0;
}

static int
cuda_pfn_func_loader(void)
{
	CUresult res;

	res = sym_cuGetProcAddress("cuGetErrorString",
			(void **) (&pfn_cuGetErrorString), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuGetErrorString failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuGetErrorName",
			(void **)(&pfn_cuGetErrorName), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuGetErrorName failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuPointerSetAttribute",
			(void **)(&pfn_cuPointerSetAttribute), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuPointerSetAttribute failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuDeviceGetAttribute",
			(void **)(&pfn_cuDeviceGetAttribute), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuDeviceGetAttribute failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuDeviceGetByPCIBusId",
			(void **)(&pfn_cuDeviceGetByPCIBusId), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuDeviceGetByPCIBusId failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuDeviceGetName",
			(void **)(&pfn_cuDeviceGetName), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuDeviceGetName failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuDevicePrimaryCtxRetain",
			(void **)(&pfn_cuDevicePrimaryCtxRetain), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuDevicePrimaryCtxRetain failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuDevicePrimaryCtxRelease",
			(void **)(&pfn_cuDevicePrimaryCtxRelease), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuDevicePrimaryCtxRelease failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuDeviceTotalMem",
			(void **)(&pfn_cuDeviceTotalMem), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuDeviceTotalMem failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuCtxGetApiVersion",
			(void **)(&pfn_cuCtxGetApiVersion), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuCtxGetApiVersion failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuCtxGetDevice",
			(void **)(&pfn_cuCtxGetDevice), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuCtxGetDevice failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuCtxSetCurrent",
			(void **)(&pfn_cuCtxSetCurrent), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuCtxSetCurrent failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuCtxGetCurrent",
			(void **)(&pfn_cuCtxGetCurrent), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuCtxGetCurrent failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuCtxGetExecAffinity",
			(void **)(&pfn_cuCtxGetExecAffinity), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuCtxGetExecAffinity failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuMemAlloc",
			(void **)(&pfn_cuMemAlloc), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuMemAlloc failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuMemFree",
			(void **)(&pfn_cuMemFree), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuMemFree failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuMemHostRegister",
			(void **)(&pfn_cuMemHostRegister), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuMemHostRegister failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuMemHostUnregister",
			(void **)(&pfn_cuMemHostUnregister), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuMemHostUnregister failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuMemHostGetDevicePointer",
			(void **)(&pfn_cuMemHostGetDevicePointer), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve pfn_cuMemHostGetDevicePointer failed with %d", res);
		return -1;
	}

	res = sym_cuGetProcAddress("cuFlushGPUDirectRDMAWrites",
			(void **)(&pfn_cuFlushGPUDirectRDMAWrites), cuda_driver_version, 0);
	if (res != 0) {
		rte_cuda_log(ERR, "Retrieve cuFlushGPUDirectRDMAWrites failed with %d", res);
		return -1;
	}

	return 0;
}

/* Generate a key from a memory pointer */
static cuda_ptr_key
get_hash_from_ptr(void *ptr)
{
	return (uintptr_t)ptr;
}

static uint32_t
mem_list_count_item(void)
{
	return mem_alloc_list_last_elem;
}

/* Initiate list of memory allocations if not done yet */
static struct mem_entry *
mem_list_add_item(void)
{
	/* Initiate list of memory allocations if not done yet */
	if (mem_alloc_list_head == NULL) {
		mem_alloc_list_head = rte_zmalloc(NULL,
				sizeof(struct mem_entry),
				RTE_CACHE_LINE_SIZE);
		if (mem_alloc_list_head == NULL) {
			rte_cuda_log(ERR, "Failed to allocate memory for memory list");
			return NULL;
		}

		mem_alloc_list_head->next = NULL;
		mem_alloc_list_head->prev = NULL;
		mem_alloc_list_tail = mem_alloc_list_head;
	} else {
		struct mem_entry *mem_alloc_list_cur = rte_zmalloc(NULL,
				sizeof(struct mem_entry),
				RTE_CACHE_LINE_SIZE);

		if (mem_alloc_list_cur == NULL) {
			rte_cuda_log(ERR, "Failed to allocate memory for memory list");
			return NULL;
		}

		mem_alloc_list_tail->next = mem_alloc_list_cur;
		mem_alloc_list_cur->prev = mem_alloc_list_tail;
		mem_alloc_list_tail = mem_alloc_list_tail->next;
		mem_alloc_list_tail->next = NULL;
	}

	mem_alloc_list_last_elem++;

	return mem_alloc_list_tail;
}

static struct mem_entry *
mem_list_find_item(cuda_ptr_key pk)
{
	struct mem_entry *mem_alloc_list_cur = NULL;

	if (mem_alloc_list_head == NULL) {
		rte_cuda_log(ERR, "Memory list doesn't exist");
		return NULL;
	}

	if (mem_list_count_item() == 0) {
		rte_cuda_log(ERR, "No items in memory list");
		return NULL;
	}

	mem_alloc_list_cur = mem_alloc_list_head;

	while (mem_alloc_list_cur != NULL) {
		if (mem_alloc_list_cur->pkey == pk)
			return mem_alloc_list_cur;
		mem_alloc_list_cur = mem_alloc_list_cur->next;
	}

	return mem_alloc_list_cur;
}

static int
mem_list_del_item(cuda_ptr_key pk)
{
	struct mem_entry *mem_alloc_list_cur = NULL;

	mem_alloc_list_cur = mem_list_find_item(pk);
	if (mem_alloc_list_cur == NULL)
		return -EINVAL;

	/* if key is in head */
	if (mem_alloc_list_cur->prev == NULL) {
		mem_alloc_list_head = mem_alloc_list_cur->next;
		if (mem_alloc_list_head != NULL)
			mem_alloc_list_head->prev = NULL;
	} else {
		mem_alloc_list_cur->prev->next = mem_alloc_list_cur->next;
		if (mem_alloc_list_cur->next != NULL)
			mem_alloc_list_cur->next->prev = mem_alloc_list_cur->prev;
	}

	rte_free(mem_alloc_list_cur);

	mem_alloc_list_last_elem--;

	return 0;
}

static int
cuda_dev_info_get(struct rte_gpu *dev, struct rte_gpu_info *info)
{
	int ret = 0;
	CUresult res;
	struct rte_gpu_info parent_info;
	CUexecAffinityParam affinityPrm;
	const char *err_string;
	struct cuda_info *private;
	CUcontext current_ctx;
	CUcontext input_ctx;

	if (dev == NULL) {
		rte_errno = ENODEV;
		return -rte_errno;
	}

	/* Child initialization time probably called by rte_gpu_add_child() */
	if (dev->mpshared->info.parent != RTE_GPU_ID_NONE &&
			dev->mpshared->dev_private == NULL) {
		/* Store current ctx */
		res = pfn_cuCtxGetCurrent(&current_ctx);
		if (res != 0) {
			pfn_cuGetErrorString(res, &(err_string));
			rte_cuda_log(ERR, "cuCtxGetCurrent failed with %s",
					err_string);
			rte_errno = EPERM;
			return -rte_errno;
		}

		/* Set child ctx as current ctx */
		input_ctx = (CUcontext)((uintptr_t)dev->mpshared->info.context);
		res = pfn_cuCtxSetCurrent(input_ctx);
		if (res != 0) {
			pfn_cuGetErrorString(res, &(err_string));
			rte_cuda_log(ERR, "cuCtxSetCurrent input failed with %s",
					err_string);
			rte_errno = EPERM;
			return -rte_errno;
		}

		/*
		 * Ctx capacity info
		 */

		/* MPS compatible */
		res = pfn_cuCtxGetExecAffinity(&affinityPrm,
				CU_EXEC_AFFINITY_TYPE_SM_COUNT);
		if (res != 0) {
			pfn_cuGetErrorString(res, &(err_string));
			rte_cuda_log(ERR, "cuCtxGetExecAffinity failed with %s",
					err_string);
		}
		dev->mpshared->info.processor_count =
				(uint32_t)affinityPrm.param.smCount.val;

		ret = rte_gpu_info_get(dev->mpshared->info.parent, &parent_info);
		if (ret) {
			rte_errno = ENODEV;
			return -rte_errno;
		}
		dev->mpshared->info.total_memory = parent_info.total_memory;

		dev->mpshared->info.page_size = parent_info.page_size;

		/*
		 * GPU Device private info
		 */
		dev->mpshared->dev_private = rte_zmalloc(NULL,
				sizeof(struct cuda_info),
				RTE_CACHE_LINE_SIZE);
		if (dev->mpshared->dev_private == NULL) {
			rte_cuda_log(ERR, "Failed to allocate memory for GPU process private");
			rte_errno = EPERM;
			return -rte_errno;
		}

		private = (struct cuda_info *)dev->mpshared->dev_private;

		res = pfn_cuCtxGetDevice(&(private->cu_dev));
		if (res != 0) {
			pfn_cuGetErrorString(res, &(err_string));
			rte_cuda_log(ERR, "cuCtxGetDevice failed with %s",
					err_string);
			rte_errno = EPERM;
			return -rte_errno;
		}

		res = pfn_cuDeviceGetName(private->gpu_name,
				RTE_DEV_NAME_MAX_LEN, private->cu_dev);
		if (res != 0) {
			pfn_cuGetErrorString(res, &(err_string));
			rte_cuda_log(ERR, "cuDeviceGetName failed with %s",
					err_string);
			rte_errno = EPERM;
			return -rte_errno;
		}

		/* Restore original ctx as current ctx */
		res = pfn_cuCtxSetCurrent(current_ctx);
		if (res != 0) {
			pfn_cuGetErrorString(res, &(err_string));
			rte_cuda_log(ERR, "cuCtxSetCurrent current failed with %s",
					err_string);
			rte_errno = EPERM;
			return -rte_errno;
		}
	}

	*info = dev->mpshared->info;

	return 0;
}

/*
 * GPU Memory
 */

static int
cuda_mem_alloc(struct rte_gpu *dev, size_t size, unsigned int align, void **ptr)
{
	CUresult res;
	const char *err_string;
	CUcontext current_ctx;
	CUcontext input_ctx;
	unsigned int flag = 1;

	if (dev == NULL)
		return -ENODEV;

	/* Store current ctx */
	res = pfn_cuCtxGetCurrent(&current_ctx);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuCtxGetCurrent failed with %s",
				err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	/* Set child ctx as current ctx */
	input_ctx = (CUcontext)((uintptr_t)dev->mpshared->info.context);
	res = pfn_cuCtxSetCurrent(input_ctx);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuCtxSetCurrent input failed with %s",
				err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	/* Get next memory list item */
	mem_alloc_list_tail = mem_list_add_item();
	if (mem_alloc_list_tail == NULL) {
		rte_errno = EPERM;
		return -rte_errno;
	}

	/* Allocate memory */
	mem_alloc_list_tail->size = size;
	mem_alloc_list_tail->size_orig = size + align;

	res = pfn_cuMemAlloc(&(mem_alloc_list_tail->ptr_orig_d),
			mem_alloc_list_tail->size_orig);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuCtxSetCurrent current failed with %s",
				err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	/* Align memory address */
	mem_alloc_list_tail->ptr_d = mem_alloc_list_tail->ptr_orig_d;
	if (align && ((uintptr_t)mem_alloc_list_tail->ptr_d) % align)
		mem_alloc_list_tail->ptr_d += (align -
				(((uintptr_t)mem_alloc_list_tail->ptr_d) % align));

	/* GPUDirect RDMA attribute required */
	res = pfn_cuPointerSetAttribute(&flag,
			CU_POINTER_ATTRIBUTE_SYNC_MEMOPS,
			mem_alloc_list_tail->ptr_d);
	if (res != 0) {
		rte_cuda_log(ERR, "Could not set SYNC MEMOP attribute for "
				"GPU memory at  %"PRIu32", err %d",
				(uint32_t)mem_alloc_list_tail->ptr_d, res);
		rte_errno = EPERM;
		return -rte_errno;
	}

	mem_alloc_list_tail->pkey = get_hash_from_ptr((void *)mem_alloc_list_tail->ptr_d);
	mem_alloc_list_tail->ptr_h = NULL;
	mem_alloc_list_tail->dev = dev;
	mem_alloc_list_tail->ctx = (CUcontext)((uintptr_t)dev->mpshared->info.context);
	mem_alloc_list_tail->mtype = GPU_MEM;

	/* Restore original ctx as current ctx */
	res = pfn_cuCtxSetCurrent(current_ctx);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuCtxSetCurrent current failed with %s",
				err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	*ptr = (void *)mem_alloc_list_tail->ptr_d;

	return 0;
}

static int
cuda_mem_register(struct rte_gpu *dev, size_t size, void *ptr)
{
	CUresult res;
	const char *err_string;
	CUcontext current_ctx;
	CUcontext input_ctx;
	unsigned int flag = 1;
	int use_ptr_h = 0;

	if (dev == NULL)
		return -ENODEV;

	/* Store current ctx */
	res = pfn_cuCtxGetCurrent(&current_ctx);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuCtxGetCurrent failed with %s",
				err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	/* Set child ctx as current ctx */
	input_ctx = (CUcontext)((uintptr_t)dev->mpshared->info.context);
	res = pfn_cuCtxSetCurrent(input_ctx);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuCtxSetCurrent input failed with %s",
				err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	/* Get next memory list item */
	mem_alloc_list_tail = mem_list_add_item();
	if (mem_alloc_list_tail == NULL) {
		rte_errno = EPERM;
		return -rte_errno;
	}

	/* Allocate memory */
	mem_alloc_list_tail->size = size;
	mem_alloc_list_tail->ptr_h = ptr;

	res = pfn_cuMemHostRegister(mem_alloc_list_tail->ptr_h,
			mem_alloc_list_tail->size,
			CU_MEMHOSTREGISTER_PORTABLE |
			CU_MEMHOSTREGISTER_DEVICEMAP);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuMemHostRegister failed with %s ptr %p size %zd",
				err_string,
				mem_alloc_list_tail->ptr_h,
				mem_alloc_list_tail->size);
		rte_errno = EPERM;
		return -rte_errno;
	}

	res = pfn_cuDeviceGetAttribute(&(use_ptr_h),
			CU_DEVICE_ATTRIBUTE_CAN_USE_HOST_POINTER_FOR_REGISTERED_MEM,
			((struct cuda_info *)(dev->mpshared->dev_private))->cu_dev);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuDeviceGetAttribute failed with %s",
				err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	if (use_ptr_h == 0) {
		res = pfn_cuMemHostGetDevicePointer(&(mem_alloc_list_tail->ptr_d),
				mem_alloc_list_tail->ptr_h, 0);
		if (res != 0) {
			pfn_cuGetErrorString(res, &(err_string));
			rte_cuda_log(ERR, "cuMemHostGetDevicePointer failed with %s",
					err_string);
			rte_errno = EPERM;
			return -rte_errno;
		}

		if ((uintptr_t)mem_alloc_list_tail->ptr_d !=
				(uintptr_t)mem_alloc_list_tail->ptr_h) {
			rte_cuda_log(ERR, "Host input pointer is different wrt GPU registered pointer");
			rte_errno = ENOTSUP;
			return -rte_errno;
		}
	} else {
		mem_alloc_list_tail->ptr_d = (CUdeviceptr)mem_alloc_list_tail->ptr_h;
	}

	/* GPUDirect RDMA attribute required */
	res = pfn_cuPointerSetAttribute(&flag,
			CU_POINTER_ATTRIBUTE_SYNC_MEMOPS,
			mem_alloc_list_tail->ptr_d);
	if (res != 0) {
		rte_cuda_log(ERR, "Could not set SYNC MEMOP attribute for GPU memory at %"PRIu32
				", err %d", (uint32_t)mem_alloc_list_tail->ptr_d, res);
		rte_errno = EPERM;
		return -rte_errno;
	}

	mem_alloc_list_tail->pkey = get_hash_from_ptr((void *)mem_alloc_list_tail->ptr_h);
	mem_alloc_list_tail->size = size;
	mem_alloc_list_tail->dev = dev;
	mem_alloc_list_tail->ctx = (CUcontext)((uintptr_t)dev->mpshared->info.context);
	mem_alloc_list_tail->mtype = CPU_REGISTERED;
	mem_alloc_list_tail->ptr_orig_d = mem_alloc_list_tail->ptr_d;

	/* Restore original ctx as current ctx */
	res = pfn_cuCtxSetCurrent(current_ctx);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuCtxSetCurrent current failed with %s",
				err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	return 0;
}

static int
cuda_mem_cpu_map(struct rte_gpu *dev, __rte_unused size_t size, void *ptr_in, void **ptr_out)
{
	struct mem_entry *mem_item;
	cuda_ptr_key hk;

	if (dev == NULL)
		return -ENODEV;

	hk = get_hash_from_ptr((void *)ptr_in);

	mem_item = mem_list_find_item(hk);
	if (mem_item == NULL) {
		rte_cuda_log(ERR, "Memory address 0x%p not found in driver memory.", ptr_in);
		rte_errno = EPERM;
		return -rte_errno;
	}

	if (mem_item->mtype != GPU_MEM) {
		rte_cuda_log(ERR, "Memory address 0x%p is not GPU memory type.", ptr_in);
		rte_errno = EPERM;
		return -rte_errno;
	}

	if (mem_item->size != size)
		rte_cuda_log(WARNING,
				"Can't expose memory area with size (%zd) different from original size (%zd).",
				size, mem_item->size);

	if (gdrcopy_pin(&gdrc_h, &(mem_item->mh), (uint64_t)mem_item->ptr_d,
					mem_item->size, &(mem_item->ptr_h))) {
		rte_cuda_log(ERR, "Error exposing GPU memory address 0x%p.", ptr_in);
		rte_errno = EPERM;
		return -rte_errno;
	}

	mem_item->mtype = GPU_REGISTERED;
	*ptr_out = mem_item->ptr_h;

	return 0;
}

static int
cuda_mem_unregister(struct rte_gpu *dev, void *ptr)
{
	CUresult res;
	struct mem_entry *mem_item;
	const char *err_string;
	cuda_ptr_key hk;

	if (dev == NULL)
		return -ENODEV;

	hk = get_hash_from_ptr((void *)ptr);

	mem_item = mem_list_find_item(hk);
	if (mem_item == NULL) {
		rte_cuda_log(ERR, "Memory address 0x%p not found in driver memory", ptr);
		rte_errno = EPERM;
		return -rte_errno;
	}

	if (mem_item->mtype == CPU_REGISTERED) {
		res = pfn_cuMemHostUnregister(ptr);
		if (res != 0) {
			pfn_cuGetErrorString(res, &(err_string));
			rte_cuda_log(ERR, "cuMemHostUnregister current failed with %s",
					err_string);
			rte_errno = EPERM;
			return -rte_errno;
		}

		return mem_list_del_item(hk);
	}

	rte_cuda_log(ERR, "Memory type %d not supported", mem_item->mtype);

	rte_errno = EPERM;
	return -rte_errno;
}

static int
cuda_mem_cpu_unmap(struct rte_gpu *dev, void *ptr_in)
{
	struct mem_entry *mem_item;
	cuda_ptr_key hk;

	if (dev == NULL)
		return -ENODEV;

	hk = get_hash_from_ptr((void *)ptr_in);

	mem_item = mem_list_find_item(hk);
	if (mem_item == NULL) {
		rte_cuda_log(ERR, "Memory address 0x%p not found in driver memory.", ptr_in);
		rte_errno = EPERM;
		return -rte_errno;
	}

	if (mem_item->mtype == GPU_REGISTERED) {
		if (gdrcopy_unpin(gdrc_h, mem_item->mh, (void *)mem_item->ptr_d,
				mem_item->size)) {
			rte_cuda_log(ERR, "Error unexposing GPU memory address 0x%p.", ptr_in);
			rte_errno = EPERM;
			return -rte_errno;
		}

		mem_item->mtype = GPU_MEM;
	} else {
		rte_errno = EPERM;
		return -rte_errno;
	}

	return 0;
}

static int
cuda_mem_free(struct rte_gpu *dev, void *ptr)
{
	CUresult res;
	struct mem_entry *mem_item;
	const char *err_string;
	cuda_ptr_key hk;

	if (dev == NULL)
		return -ENODEV;

	hk = get_hash_from_ptr((void *)ptr);

	mem_item = mem_list_find_item(hk);
	if (mem_item == NULL) {
		rte_cuda_log(ERR, "Memory address 0x%p not found in driver memory", ptr);
		rte_errno = EPERM;
		return -rte_errno;
	}

	/*
	 * If a GPU memory area that's CPU mapped is being freed
	 * without calling cpu_unmap, force the unmapping.
	 */
	if (mem_item->mtype == GPU_REGISTERED)
		cuda_mem_cpu_unmap(dev, ptr);

	if (mem_item->mtype == GPU_MEM) {
		res = pfn_cuMemFree(mem_item->ptr_orig_d);
		if (res != 0) {
			pfn_cuGetErrorString(res, &(err_string));
			rte_cuda_log(ERR, "cuMemFree current failed with %s",
					err_string);
			rte_errno = EPERM;
			return -rte_errno;
		}

		return mem_list_del_item(hk);
	}

	rte_cuda_log(ERR, "Memory type %d not supported", mem_item->mtype);

	return -EPERM;
}

static int
cuda_dev_close(struct rte_gpu *dev)
{
	if (dev == NULL)
		return -EINVAL;

	rte_free(dev->mpshared->dev_private);

	return 0;
}

static int
cuda_wmb(struct rte_gpu *dev)
{
	CUresult res;
	const char *err_string;
	CUcontext current_ctx;
	CUcontext input_ctx;
	struct cuda_info *private;

	if (dev == NULL) {
		rte_errno = ENODEV;
		return -rte_errno;
	}

	private = (struct cuda_info *)dev->mpshared->dev_private;

	if (private->gdr_write_ordering != CU_GPU_DIRECT_RDMA_WRITES_ORDERING_NONE) {
		/*
		 * No need to explicitly force the write ordering because
		 * the device natively supports it
		 */
		return 0;
	}

	if (private->gdr_flush_type != CU_FLUSH_GPU_DIRECT_RDMA_WRITES_OPTION_HOST) {
		/*
		 * Can't flush GDR writes with cuFlushGPUDirectRDMAWrites CUDA function.
		 * Application needs to use alternative methods.
		 */
		rte_cuda_log(WARNING, "Can't flush GDR writes with cuFlushGPUDirectRDMAWrites CUDA function."
				"Application needs to use alternative methods.");

		rte_errno = ENOTSUP;
		return -rte_errno;
	}

	/* Store current ctx */
	res = pfn_cuCtxGetCurrent(&current_ctx);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuCtxGetCurrent failed with %s",
				err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	/* Set child ctx as current ctx */
	input_ctx = (CUcontext)((uintptr_t)dev->mpshared->info.context);
	res = pfn_cuCtxSetCurrent(input_ctx);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuCtxSetCurrent input failed with %s",
				err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	res = pfn_cuFlushGPUDirectRDMAWrites(CU_FLUSH_GPU_DIRECT_RDMA_WRITES_TARGET_CURRENT_CTX,
			CU_FLUSH_GPU_DIRECT_RDMA_WRITES_TO_ALL_DEVICES);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuFlushGPUDirectRDMAWrites current failed with %s",
				err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	/* Restore original ctx as current ctx */
	res = pfn_cuCtxSetCurrent(current_ctx);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuCtxSetCurrent current failed with %s",
				err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	return 0;
}

static int
cuda_gpu_probe(__rte_unused struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	struct rte_gpu *dev = NULL;
	CUresult res;
	CUdevice cu_dev_id;
	CUcontext pctx;
	char dev_name[RTE_DEV_NAME_MAX_LEN];
	const char *err_string;
	int processor_count = 0;
	struct cuda_info *private;

	if (pci_dev == NULL) {
		rte_cuda_log(ERR, "NULL PCI device");
		rte_errno = ENODEV;
		return -rte_errno;
	}

	rte_pci_device_name(&pci_dev->addr, dev_name, sizeof(dev_name));

	/* Allocate memory to be used privately by drivers */
	dev = rte_gpu_allocate(pci_dev->device.name);
	if (dev == NULL) {
		rte_errno = ENODEV;
		return -rte_errno;
	}

	/* Initialize values only for the first CUDA driver call */
	if (dev->mpshared->info.dev_id == 0) {
		mem_alloc_list_head = NULL;
		mem_alloc_list_tail = NULL;
		mem_alloc_list_last_elem = 0;

		/* Load libcuda.so library */
		if (cuda_loader()) {
			rte_cuda_log(ERR, "CUDA Driver library not found");
			rte_errno = ENOTSUP;
			return -rte_errno;
		}

		/* Load initial CUDA functions */
		if (cuda_sym_func_loader()) {
			rte_cuda_log(ERR, "CUDA functions not found in library");
			rte_errno = ENOTSUP;
			return -rte_errno;
		}

		/*
		 * Required to initialize the CUDA Driver.
		 * Multiple calls of cuInit() will return immediately
		 * without making any relevant change
		 */
		sym_cuInit(0);

		res = sym_cuDriverGetVersion(&cuda_driver_version);
		if (res != 0) {
			rte_cuda_log(ERR, "cuDriverGetVersion failed with %d", res);
			rte_errno = ENOTSUP;
			return -rte_errno;
		}

		if (cuda_driver_version < CUDA_DRIVER_MIN_VERSION) {
			rte_cuda_log(ERR, "CUDA Driver version found is %d. "
					"Minimum requirement is %d",
					cuda_driver_version,
					CUDA_DRIVER_MIN_VERSION);
			rte_errno = ENOTSUP;
			return -rte_errno;
		}

		if (cuda_pfn_func_loader()) {
			rte_cuda_log(ERR, "CUDA PFN functions not found in library");
			rte_errno = ENOTSUP;
			return -rte_errno;
		}

		gdrc_h = NULL;
	}

	/* Fill HW specific part of device structure */
	dev->device = &pci_dev->device;
	dev->mpshared->info.numa_node = pci_dev->device.numa_node;

	/* Get NVIDIA GPU Device descriptor */
	res = pfn_cuDeviceGetByPCIBusId(&cu_dev_id, dev->device->name);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuDeviceGetByPCIBusId name %s failed with %d: %s",
				dev->device->name, res, err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	res = pfn_cuDevicePrimaryCtxRetain(&pctx, cu_dev_id);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuDevicePrimaryCtxRetain name %s failed with %d: %s",
				dev->device->name, res, err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	res = pfn_cuCtxGetApiVersion(pctx, &cuda_api_version);
	if (res != 0) {
		rte_cuda_log(ERR, "cuCtxGetApiVersion failed with %d", res);
		rte_errno = ENOTSUP;
		return -rte_errno;
	}

	if (cuda_api_version < CUDA_API_MIN_VERSION) {
		rte_cuda_log(ERR, "CUDA API version found is %d Minimum requirement is %d",
				cuda_api_version, CUDA_API_MIN_VERSION);
		rte_errno = ENOTSUP;
		return -rte_errno;
	}

	dev->mpshared->info.context = (uint64_t)pctx;

	/*
	 * GPU Device generic info
	 */

	/* Processor count */
	res = pfn_cuDeviceGetAttribute(&(processor_count),
			CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT,
			cu_dev_id);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuDeviceGetAttribute failed with %s",
				err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}
	dev->mpshared->info.processor_count = (uint32_t)processor_count;

	/* Total memory */
	res = pfn_cuDeviceTotalMem(&dev->mpshared->info.total_memory, cu_dev_id);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuDeviceTotalMem failed with %s",
				err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	dev->mpshared->info.page_size = (size_t)GPU_PAGE_SIZE;

	/*
	 * GPU Device private info
	 */
	dev->mpshared->dev_private = rte_zmalloc(NULL,
			sizeof(struct cuda_info),
			RTE_CACHE_LINE_SIZE);
	if (dev->mpshared->dev_private == NULL) {
		rte_cuda_log(ERR, "Failed to allocate memory for GPU process private");
		rte_errno = EPERM;
		return -rte_errno;
	}

	private = (struct cuda_info *)dev->mpshared->dev_private;
	private->cu_dev = cu_dev_id;
	res = pfn_cuDeviceGetName(private->gpu_name,
			RTE_DEV_NAME_MAX_LEN,
			cu_dev_id);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuDeviceGetName failed with %s",
				err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	res = pfn_cuDeviceGetAttribute(&(private->gdr_supported),
			CU_DEVICE_ATTRIBUTE_GPU_DIRECT_RDMA_SUPPORTED,
			cu_dev_id);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR, "cuDeviceGetAttribute failed with %s",
				err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	if (private->gdr_supported == 0)
		rte_cuda_log(WARNING, "GPU %s doesn't support GPUDirect RDMA",
				pci_dev->device.name);

	res = pfn_cuDeviceGetAttribute(&(private->gdr_write_ordering),
			CU_DEVICE_ATTRIBUTE_GPU_DIRECT_RDMA_WRITES_ORDERING,
			cu_dev_id);
	if (res != 0) {
		pfn_cuGetErrorString(res, &(err_string));
		rte_cuda_log(ERR,
				"cuDeviceGetAttribute failed with %s",
				err_string);
		rte_errno = EPERM;
		return -rte_errno;
	}

	if (private->gdr_write_ordering == CU_GPU_DIRECT_RDMA_WRITES_ORDERING_NONE) {
		res = pfn_cuDeviceGetAttribute(&(private->gdr_flush_type),
				CU_DEVICE_ATTRIBUTE_GPU_DIRECT_RDMA_FLUSH_WRITES_OPTIONS,
				cu_dev_id);
		if (res != 0) {
			pfn_cuGetErrorString(res, &(err_string));
			rte_cuda_log(ERR, "cuDeviceGetAttribute failed with %s",
					err_string);
			rte_errno = EPERM;
			return -rte_errno;
		}

		if (private->gdr_flush_type != CU_FLUSH_GPU_DIRECT_RDMA_WRITES_OPTION_HOST)
			rte_cuda_log(ERR, "GPUDirect RDMA flush writes API is not supported");
	}

	dev->ops.dev_info_get = cuda_dev_info_get;
	dev->ops.dev_close = cuda_dev_close;
	dev->ops.mem_alloc = cuda_mem_alloc;
	dev->ops.mem_free = cuda_mem_free;
	dev->ops.mem_register = cuda_mem_register;
	dev->ops.mem_unregister = cuda_mem_unregister;
	dev->ops.mem_cpu_map = cuda_mem_cpu_map;
	dev->ops.mem_cpu_unmap = cuda_mem_cpu_unmap;
	dev->ops.wmb = cuda_wmb;

	rte_gpu_complete_new(dev);

	rte_cuda_debug("dev id = %u name = %s",
			dev->mpshared->info.dev_id, private->gpu_name);

	return 0;
}

static int
cuda_gpu_remove(struct rte_pci_device *pci_dev)
{
	struct rte_gpu *dev;
	int ret;
	uint8_t gpu_id;

	if (pci_dev == NULL) {
		rte_errno = ENODEV;
		return -rte_errno;
	}

	dev = rte_gpu_get_by_name(pci_dev->device.name);
	if (dev == NULL) {
		rte_cuda_log(ERR, "Couldn't find HW dev \"%s\" to uninitialise it",
				pci_dev->device.name);
		rte_errno = ENODEV;
		return -rte_errno;
	}
	gpu_id = dev->mpshared->info.dev_id;

	/* release dev from library */
	ret = rte_gpu_release(dev);
	if (ret)
		rte_cuda_log(ERR, "Device %i failed to uninit: %i", gpu_id, ret);

	rte_cuda_debug("Destroyed dev = %u", gpu_id);

	return 0;
}

static struct rte_pci_driver rte_cuda_driver = {
	.id_table = pci_id_cuda_map,
	.drv_flags = RTE_PCI_DRV_WC_ACTIVATE,
	.probe = cuda_gpu_probe,
	.remove = cuda_gpu_remove,
};

RTE_PMD_REGISTER_PCI(gpu_cuda, rte_cuda_driver);
RTE_PMD_REGISTER_PCI_TABLE(gpu_cuda, pci_id_cuda_map);
RTE_PMD_REGISTER_KMOD_DEP(gpu_cuda, "* nvidia & (nv_peer_mem | nvpeer_mem)");
