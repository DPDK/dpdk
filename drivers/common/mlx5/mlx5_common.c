/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <rte_errno.h>
#include <rte_mempool.h>
#include <rte_malloc.h>

#include "mlx5_common.h"
#include "mlx5_common_os.h"
#include "mlx5_common_utils.h"
#include "mlx5_malloc.h"
#include "mlx5_common_pci.h"

int mlx5_common_logtype;

uint8_t haswell_broadwell_cpu;

/* In case this is an x86_64 intel processor to check if
 * we should use relaxed ordering.
 */
#ifdef RTE_ARCH_X86_64
/**
 * This function returns processor identification and feature information
 * into the registers.
 *
 * @param eax, ebx, ecx, edx
 *		Pointers to the registers that will hold cpu information.
 * @param level
 *		The main category of information returned.
 */
static inline void mlx5_cpu_id(unsigned int level,
				unsigned int *eax, unsigned int *ebx,
				unsigned int *ecx, unsigned int *edx)
{
	__asm__("cpuid\n\t"
		: "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
		: "0" (level));
}
#endif

RTE_INIT_PRIO(mlx5_log_init, LOG)
{
	mlx5_common_logtype = rte_log_register("pmd.common.mlx5");
	if (mlx5_common_logtype >= 0)
		rte_log_set_level(mlx5_common_logtype, RTE_LOG_NOTICE);
}

static bool mlx5_common_initialized;

/**
 * One time innitialization routine for run-time dependency on glue library
 * for multiple PMDs. Each mlx5 PMD that depends on mlx5_common module,
 * must invoke in its constructor.
 */
void
mlx5_common_init(void)
{
	if (mlx5_common_initialized)
		return;

	mlx5_glue_constructor();
	mlx5_common_pci_init();
	mlx5_common_initialized = true;
}

/**
 * This function is responsible of initializing the variable
 *  haswell_broadwell_cpu by checking if the cpu is intel
 *  and reading the data returned from mlx5_cpu_id().
 *  since haswell and broadwell cpus don't have improved performance
 *  when using relaxed ordering we want to check the cpu type before
 *  before deciding whether to enable RO or not.
 *  if the cpu is haswell or broadwell the variable will be set to 1
 *  otherwise it will be 0.
 */
RTE_INIT_PRIO(mlx5_is_haswell_broadwell_cpu, LOG)
{
#ifdef RTE_ARCH_X86_64
	unsigned int broadwell_models[4] = {0x3d, 0x47, 0x4F, 0x56};
	unsigned int haswell_models[4] = {0x3c, 0x3f, 0x45, 0x46};
	unsigned int i, model, family, brand_id, vendor;
	unsigned int signature_intel_ebx = 0x756e6547;
	unsigned int extended_model;
	unsigned int eax = 0;
	unsigned int ebx = 0;
	unsigned int ecx = 0;
	unsigned int edx = 0;
	int max_level;

	mlx5_cpu_id(0, &eax, &ebx, &ecx, &edx);
	vendor = ebx;
	max_level = eax;
	if (max_level < 1) {
		haswell_broadwell_cpu = 0;
		return;
	}
	mlx5_cpu_id(1, &eax, &ebx, &ecx, &edx);
	model = (eax >> 4) & 0x0f;
	family = (eax >> 8) & 0x0f;
	brand_id = ebx & 0xff;
	extended_model = (eax >> 12) & 0xf0;
	/* Check if the processor is Haswell or Broadwell */
	if (vendor == signature_intel_ebx) {
		if (family == 0x06)
			model += extended_model;
		if (brand_id == 0 && family == 0x6) {
			for (i = 0; i < RTE_DIM(broadwell_models); i++)
				if (model == broadwell_models[i]) {
					haswell_broadwell_cpu = 1;
					return;
				}
			for (i = 0; i < RTE_DIM(haswell_models); i++)
				if (model == haswell_models[i]) {
					haswell_broadwell_cpu = 1;
					return;
				}
		}
	}
#endif
	haswell_broadwell_cpu = 0;
}

/**
 * Allocate page of door-bells and register it using DevX API.
 *
 * @param [in] ctx
 *   Pointer to the device context.
 *
 * @return
 *   Pointer to new page on success, NULL otherwise.
 */
static struct mlx5_devx_dbr_page *
mlx5_alloc_dbr_page(void *ctx)
{
	struct mlx5_devx_dbr_page *page;

	/* Allocate space for door-bell page and management data. */
	page = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO,
			   sizeof(struct mlx5_devx_dbr_page),
			   RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (!page) {
		DRV_LOG(ERR, "cannot allocate dbr page");
		return NULL;
	}
	/* Register allocated memory. */
	page->umem = mlx5_glue->devx_umem_reg(ctx, page->dbrs,
					      MLX5_DBR_PAGE_SIZE, 0);
	if (!page->umem) {
		DRV_LOG(ERR, "cannot umem reg dbr page");
		mlx5_free(page);
		return NULL;
	}
	return page;
}

/**
 * Find the next available door-bell, allocate new page if needed.
 *
 * @param [in] ctx
 *   Pointer to device context.
 * @param [in] head
 *   Pointer to the head of dbr pages list.
 * @param [out] dbr_page
 *   Door-bell page containing the page data.
 *
 * @return
 *   Door-bell address offset on success, a negative error value otherwise.
 */
int64_t
mlx5_get_dbr(void *ctx,  struct mlx5_dbr_page_list *head,
	     struct mlx5_devx_dbr_page **dbr_page)
{
	struct mlx5_devx_dbr_page *page = NULL;
	uint32_t i, j;

	LIST_FOREACH(page, head, next)
		if (page->dbr_count < MLX5_DBR_PER_PAGE)
			break;
	if (!page) { /* No page with free door-bell exists. */
		page = mlx5_alloc_dbr_page(ctx);
		if (!page) /* Failed to allocate new page. */
			return (-1);
		LIST_INSERT_HEAD(head, page, next);
	}
	/* Loop to find bitmap part with clear bit. */
	for (i = 0;
	     i < MLX5_DBR_BITMAP_SIZE && page->dbr_bitmap[i] == UINT64_MAX;
	     i++)
		; /* Empty. */
	/* Find the first clear bit. */
	MLX5_ASSERT(i < MLX5_DBR_BITMAP_SIZE);
	j = rte_bsf64(~page->dbr_bitmap[i]);
	page->dbr_bitmap[i] |= (UINT64_C(1) << j);
	page->dbr_count++;
	*dbr_page = page;
	return (i * CHAR_BIT * sizeof(uint64_t) + j) * MLX5_DBR_SIZE;
}

/**
 * Release a door-bell record.
 *
 * @param [in] head
 *   Pointer to the head of dbr pages list.
 * @param [in] umem_id
 *   UMEM ID of page containing the door-bell record to release.
 * @param [in] offset
 *   Offset of door-bell record in page.
 *
 * @return
 *   0 on success, a negative error value otherwise.
 */
int32_t
mlx5_release_dbr(struct mlx5_dbr_page_list *head, uint32_t umem_id,
		 uint64_t offset)
{
	struct mlx5_devx_dbr_page *page = NULL;
	int ret = 0;

	LIST_FOREACH(page, head, next)
		/* Find the page this address belongs to. */
		if (mlx5_os_get_umem_id(page->umem) == umem_id)
			break;
	if (!page)
		return -EINVAL;
	page->dbr_count--;
	if (!page->dbr_count) {
		/* Page not used, free it and remove from list. */
		LIST_REMOVE(page, next);
		if (page->umem)
			ret = -mlx5_glue->devx_umem_dereg(page->umem);
		mlx5_free(page);
	} else {
		/* Mark in bitmap that this door-bell is not in use. */
		offset /= MLX5_DBR_SIZE;
		int i = offset / 64;
		int j = offset % 64;

		page->dbr_bitmap[i] &= ~(UINT64_C(1) << j);
	}
	return ret;
}
