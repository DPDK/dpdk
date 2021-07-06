/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Intel Corporation
 */

#include <stdio.h>
#include <inttypes.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_memzone.h>

#include "iavf_type.h"
#include "iavf_prototype.h"

enum iavf_status
iavf_allocate_dma_mem_d(__rte_unused struct iavf_hw *hw,
			struct iavf_dma_mem *mem,
			u64 size,
			u32 alignment)
{
	static uint64_t iavf_dma_memzone_id;
	const struct rte_memzone *mz = NULL;
	char z_name[RTE_MEMZONE_NAMESIZE];

	if (!mem)
		return IAVF_ERR_PARAM;

	snprintf(z_name, sizeof(z_name), "iavf_dma_%" PRIu64,
		__atomic_fetch_add(&iavf_dma_memzone_id, 1, __ATOMIC_RELAXED));
	mz = rte_memzone_reserve_bounded(z_name, size, SOCKET_ID_ANY,
					 RTE_MEMZONE_IOVA_CONTIG, alignment,
					 RTE_PGSIZE_2M);
	if (!mz)
		return IAVF_ERR_NO_MEMORY;

	mem->size = size;
	mem->va = mz->addr;
	mem->pa = mz->iova;
	mem->zone = (const void *)mz;

	return IAVF_SUCCESS;
}

enum iavf_status
iavf_free_dma_mem_d(__rte_unused struct iavf_hw *hw,
		    struct iavf_dma_mem *mem)
{
	if (!mem)
		return IAVF_ERR_PARAM;

	rte_memzone_free((const struct rte_memzone *)mem->zone);
	mem->zone = NULL;
	mem->va = NULL;
	mem->pa = (u64)0;

	return IAVF_SUCCESS;
}

enum iavf_status
iavf_allocate_virt_mem_d(__rte_unused struct iavf_hw *hw,
			 struct iavf_virt_mem *mem,
			 u32 size)
{
	if (!mem)
		return IAVF_ERR_PARAM;

	mem->size = size;
	mem->va = rte_zmalloc("iavf", size, 0);

	if (mem->va)
		return IAVF_SUCCESS;
	else
		return IAVF_ERR_NO_MEMORY;
}

enum iavf_status
iavf_free_virt_mem_d(__rte_unused struct iavf_hw *hw,
		     struct iavf_virt_mem *mem)
{
	if (!mem)
		return IAVF_ERR_PARAM;

	rte_free(mem->va);
	mem->va = NULL;

	return IAVF_SUCCESS;
}

RTE_LOG_REGISTER_DEFAULT(iavf_common_logger, NOTICE);
