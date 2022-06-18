/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */
#include <stdlib.h>

#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_common.h>
#include <rte_sched_common.h>

#include <mlx5_prm.h>
#include <mlx5_common.h>

#include "mlx5_vdpa_utils.h"
#include "mlx5_vdpa.h"

void
mlx5_vdpa_mem_dereg(struct mlx5_vdpa_priv *priv)
{
	struct mlx5_vdpa_query_mr *mrs =
		(struct mlx5_vdpa_query_mr *)priv->mrs;
	struct mlx5_vdpa_query_mr *entry;
	int i;

	if (priv->mrs) {
		for (i = priv->num_mrs - 1; i >= 0; i--) {
			entry = &mrs[i];
			if (entry->is_indirect) {
				if (entry->mkey)
					claim_zero(
					mlx5_devx_cmd_destroy(entry->mkey));
			} else {
				if (entry->mr)
					claim_zero(
					mlx5_glue->dereg_mr(entry->mr));
			}
		}
		rte_free(priv->mrs);
		priv->mrs = NULL;
		priv->num_mrs = 0;
	}
	if (priv->vmem_info.vmem) {
		free(priv->vmem_info.vmem);
		priv->vmem_info.vmem = NULL;
	}
	priv->gpa_mkey_index = 0;
}

static int
mlx5_vdpa_regions_addr_cmp(const void *a, const void *b)
{
	const struct rte_vhost_mem_region *region_a = a;
	const struct rte_vhost_mem_region *region_b = b;

	if (region_a->guest_phys_addr < region_b->guest_phys_addr)
		return -1;
	if (region_a->guest_phys_addr > region_b->guest_phys_addr)
		return 1;
	return 0;
}

#define KLM_NUM_MAX_ALIGN(sz) (RTE_ALIGN_CEIL(sz, MLX5_MAX_KLM_BYTE_COUNT) / \
			       MLX5_MAX_KLM_BYTE_COUNT)

/*
 * Allocate and sort the region list and choose indirect mkey mode:
 *   1. Calculate GCD, guest memory size and indirect mkey entries num per mode.
 *   2. Align GCD to the maximum allowed size(2G) and to be power of 2.
 *   2. Decide the indirect mkey mode according to the next rules:
 *         a. If both KLM_FBS entries number and KLM entries number are bigger
 *            than the maximum allowed(MLX5_DEVX_MAX_KLM_ENTRIES) - error.
 *         b. KLM mode if KLM_FBS entries number is bigger than the maximum
 *            allowed(MLX5_DEVX_MAX_KLM_ENTRIES).
 *         c. KLM mode if GCD is smaller than the minimum allowed(4K).
 *         d. KLM mode if the total size of KLM entries is in one cache line
 *            and the total size of KLM_FBS entries is not in one cache line.
 *         e. Otherwise, KLM_FBS mode.
 */
static struct rte_vhost_memory *
mlx5_vdpa_vhost_mem_regions_prepare(int vid, uint8_t *mode, uint64_t *mem_size,
				    uint64_t *gcd, uint32_t *entries_num)
{
	struct rte_vhost_memory *mem;
	uint64_t size;
	uint64_t klm_entries_num = 0;
	uint64_t klm_fbs_entries_num;
	uint32_t i;
	int ret = rte_vhost_get_mem_table(vid, &mem);

	if (ret < 0) {
		DRV_LOG(ERR, "Failed to get VM memory layout vid =%d.", vid);
		rte_errno = EINVAL;
		return NULL;
	}
	qsort(mem->regions, mem->nregions, sizeof(mem->regions[0]),
	      mlx5_vdpa_regions_addr_cmp);
	*mem_size = (mem->regions[(mem->nregions - 1)].guest_phys_addr) +
				      (mem->regions[(mem->nregions - 1)].size) -
					      (mem->regions[0].guest_phys_addr);
	*gcd = 0;
	for (i = 0; i < mem->nregions; ++i) {
		DRV_LOG(INFO,  "Region %u: HVA 0x%" PRIx64 ", GPA 0x%" PRIx64
			", size 0x%" PRIx64 ".", i,
			mem->regions[i].host_user_addr,
			mem->regions[i].guest_phys_addr, mem->regions[i].size);
		if (i > 0) {
			/* Hole handle. */
			size = mem->regions[i].guest_phys_addr -
				(mem->regions[i - 1].guest_phys_addr +
				 mem->regions[i - 1].size);
			*gcd = rte_get_gcd64(*gcd, size);
			klm_entries_num += KLM_NUM_MAX_ALIGN(size);
		}
		size = mem->regions[i].size;
		*gcd = rte_get_gcd64(*gcd, size);
		klm_entries_num += KLM_NUM_MAX_ALIGN(size);
	}
	if (*gcd > MLX5_MAX_KLM_BYTE_COUNT)
		*gcd = rte_get_gcd64(*gcd, MLX5_MAX_KLM_BYTE_COUNT);
	if (!RTE_IS_POWER_OF_2(*gcd)) {
		uint64_t candidate_gcd = rte_align64prevpow2(*gcd);

		while (candidate_gcd > 1 && (*gcd % candidate_gcd))
			candidate_gcd /= 2;
		DRV_LOG(DEBUG, "GCD 0x%" PRIx64 " is not power of 2. Adjusted "
			"GCD is 0x%" PRIx64 ".", *gcd, candidate_gcd);
		*gcd = candidate_gcd;
	}
	klm_fbs_entries_num = *mem_size / *gcd;
	if (*gcd < MLX5_MIN_KLM_FIXED_BUFFER_SIZE || klm_fbs_entries_num >
	    MLX5_DEVX_MAX_KLM_ENTRIES ||
	    ((klm_entries_num * sizeof(struct mlx5_klm)) <=
	    RTE_CACHE_LINE_SIZE && (klm_fbs_entries_num *
				    sizeof(struct mlx5_klm)) >
							RTE_CACHE_LINE_SIZE)) {
		*mode = MLX5_MKC_ACCESS_MODE_KLM;
		*entries_num = klm_entries_num;
		DRV_LOG(INFO, "Indirect mkey mode is KLM.");
	} else {
		*mode = MLX5_MKC_ACCESS_MODE_KLM_FBS;
		*entries_num = klm_fbs_entries_num;
		DRV_LOG(INFO, "Indirect mkey mode is KLM Fixed Buffer Size.");
	}
	DRV_LOG(DEBUG, "Memory registration information: nregions = %u, "
		"mem_size = 0x%" PRIx64 ", GCD = 0x%" PRIx64
		", klm_fbs_entries_num = 0x%" PRIx64 ", klm_entries_num = 0x%"
		PRIx64 ".", mem->nregions, *mem_size, *gcd, klm_fbs_entries_num,
		klm_entries_num);
	if (*entries_num > MLX5_DEVX_MAX_KLM_ENTRIES) {
		DRV_LOG(ERR, "Failed to prepare memory of vid %d - memory is "
			"too fragmented.", vid);
		free(mem);
		return NULL;
	}
	return mem;
}

static int
mlx5_vdpa_mem_cmp(struct rte_vhost_memory *mem1, struct rte_vhost_memory *mem2)
{
	uint32_t i;

	if (mem1->nregions != mem2->nregions)
		return -1;
	for (i = 0; i < mem1->nregions; i++) {
		if (mem1->regions[i].guest_phys_addr !=
		    mem2->regions[i].guest_phys_addr)
			return -1;
		if (mem1->regions[i].size != mem2->regions[i].size)
			return -1;
	}
	return 0;
}

#define KLM_SIZE_MAX_ALIGN(sz) ((sz) > MLX5_MAX_KLM_BYTE_COUNT ? \
				MLX5_MAX_KLM_BYTE_COUNT : (sz))

static int
mlx5_vdpa_create_indirect_mkey(struct mlx5_vdpa_priv *priv)
{
	struct mlx5_devx_mkey_attr mkey_attr;
	struct mlx5_vdpa_query_mr *mrs =
		(struct mlx5_vdpa_query_mr *)priv->mrs;
	struct mlx5_vdpa_query_mr *entry;
	struct rte_vhost_mem_region *reg;
	uint8_t mode = priv->vmem_info.mode;
	uint32_t entries_num = priv->vmem_info.entries_num;
	struct rte_vhost_memory *mem = priv->vmem_info.vmem;
	struct mlx5_klm klm_array[entries_num];
	uint64_t gcd = priv->vmem_info.gcd;
	int ret = -rte_errno;
	uint64_t klm_size;
	int klm_index = 0;
	uint64_t k;
	uint32_t i;

	/* If it is the last entry, create indirect mkey. */
	for (i = 0; i < mem->nregions; i++) {
		entry = &mrs[i];
		reg = &mem->regions[i];
		if (i > 0) {
			uint64_t sadd;
			uint64_t empty_region_sz = reg->guest_phys_addr -
					  (mem->regions[i - 1].guest_phys_addr +
					   mem->regions[i - 1].size);

			if (empty_region_sz > 0) {
				sadd = mem->regions[i - 1].guest_phys_addr +
				       mem->regions[i - 1].size;
				klm_size = mode == MLX5_MKC_ACCESS_MODE_KLM ?
				      KLM_SIZE_MAX_ALIGN(empty_region_sz) : gcd;
				for (k = 0; k < empty_region_sz;
				     k += klm_size) {
					klm_array[klm_index].byte_count =
						k + klm_size > empty_region_sz ?
						 empty_region_sz - k : klm_size;
					klm_array[klm_index].mkey =
							    priv->null_mr->lkey;
					klm_array[klm_index].address = sadd + k;
					klm_index++;
				}
			}
		}
		klm_size = mode == MLX5_MKC_ACCESS_MODE_KLM ?
					    KLM_SIZE_MAX_ALIGN(reg->size) : gcd;
		for (k = 0; k < reg->size; k += klm_size) {
			klm_array[klm_index].byte_count = k + klm_size >
					   reg->size ? reg->size - k : klm_size;
			klm_array[klm_index].mkey = entry->mr->lkey;
			klm_array[klm_index].address = reg->guest_phys_addr + k;
			klm_index++;
		}
	}
	memset(&mkey_attr, 0, sizeof(mkey_attr));
	mkey_attr.addr = (uintptr_t)(mem->regions[0].guest_phys_addr);
	mkey_attr.size = priv->vmem_info.size;
	mkey_attr.pd = priv->cdev->pdn;
	mkey_attr.umem_id = 0;
	/* Must be zero for KLM mode. */
	mkey_attr.log_entity_size = mode == MLX5_MKC_ACCESS_MODE_KLM_FBS ?
							  rte_log2_u64(gcd) : 0;
	mkey_attr.pg_access = 0;
	mkey_attr.klm_array = klm_array;
	mkey_attr.klm_num = klm_index;
	entry = &mrs[mem->nregions];
	entry->mkey = mlx5_devx_cmd_mkey_create(priv->cdev->ctx, &mkey_attr);
	if (!entry->mkey) {
		DRV_LOG(ERR, "Failed to create indirect Mkey.");
		rte_errno = -ret;
		return ret;
	}
	entry->is_indirect = 1;
	priv->gpa_mkey_index = entry->mkey->id;
	return 0;
}

/*
 * The target here is to group all the physical memory regions of the
 * virtio device in one indirect mkey.
 * For KLM Fixed Buffer Size mode (HW find the translation entry in one
 * read according to the guest phisical address):
 * All the sub-direct mkeys of it must be in the same size, hence, each
 * one of them should be in the GCD size of all the virtio memory
 * regions and the holes between them.
 * For KLM mode (each entry may be in different size so HW must iterate
 * the entries):
 * Each virtio memory region and each hole between them have one entry,
 * just need to cover the maximum allowed size(2G) by splitting entries
 * which their associated memory regions are bigger than 2G.
 * It means that each virtio memory region may be mapped to more than
 * one direct mkey in the 2 modes.
 * All the holes of invalid memory between the virtio memory regions
 * will be mapped to the null memory region for security.
 */
int
mlx5_vdpa_mem_register(struct mlx5_vdpa_priv *priv)
{
	void *mrs;
	uint8_t mode = 0;
	int ret = -rte_errno;
	uint32_t i, thrd_idx, data[1];
	uint32_t remaining_cnt = 0, err_cnt = 0, task_num = 0;
	struct rte_vhost_memory *mem = mlx5_vdpa_vhost_mem_regions_prepare
			(priv->vid, &mode, &priv->vmem_info.size,
			&priv->vmem_info.gcd, &priv->vmem_info.entries_num);

	if (!mem)
		return -rte_errno;
	if (priv->vmem_info.vmem != NULL) {
		if (mlx5_vdpa_mem_cmp(mem, priv->vmem_info.vmem) == 0) {
			/* VM memory not changed, reuse resources. */
			free(mem);
			return 0;
		}
		mlx5_vdpa_mem_dereg(priv);
	}
	priv->vmem_info.vmem = mem;
	priv->vmem_info.mode = mode;
	priv->num_mrs = mem->nregions;
	if (!priv->num_mrs || priv->num_mrs >= MLX5_VDPA_MAX_MRS) {
		DRV_LOG(ERR,
		"Invalid number of memory regions.");
		goto error;
	}
	/* The last one is indirect mkey entry. */
	priv->num_mrs++;
	mrs = rte_zmalloc("mlx5 vDPA memory regions",
		sizeof(struct mlx5_vdpa_query_mr) * priv->num_mrs, 0);
	priv->mrs = mrs;
	if (!priv->mrs) {
		DRV_LOG(ERR, "Failed to allocate private memory regions.");
		goto error;
	}
	if (priv->use_c_thread) {
		uint32_t main_task_idx[mem->nregions];

		for (i = 0; i < mem->nregions; i++) {
			thrd_idx = i % (conf_thread_mng.max_thrds + 1);
			if (!thrd_idx) {
				main_task_idx[task_num] = i;
				task_num++;
				continue;
			}
			thrd_idx = priv->last_c_thrd_idx + 1;
			if (thrd_idx >= conf_thread_mng.max_thrds)
				thrd_idx = 0;
			priv->last_c_thrd_idx = thrd_idx;
			data[0] = i;
			if (mlx5_vdpa_task_add(priv, thrd_idx,
				MLX5_VDPA_TASK_REG_MR,
				&remaining_cnt, &err_cnt,
				(void **)&data, 1)) {
				DRV_LOG(ERR,
				"Fail to add task mem region (%d)", i);
				main_task_idx[task_num] = i;
				task_num++;
			}
		}
		for (i = 0; i < task_num; i++) {
			ret = mlx5_vdpa_register_mr(priv,
					main_task_idx[i]);
			if (ret) {
				DRV_LOG(ERR,
				"Failed to register mem region %d.", i);
				goto error;
			}
		}
		if (mlx5_vdpa_c_thread_wait_bulk_tasks_done(&remaining_cnt,
			&err_cnt, 100)) {
			DRV_LOG(ERR,
			"Failed to wait register mem region tasks ready.");
			goto error;
		}
	} else {
		for (i = 0; i < mem->nregions; i++) {
			ret = mlx5_vdpa_register_mr(priv, i);
			if (ret) {
				DRV_LOG(ERR,
				"Failed to register mem region %d.", i);
				goto error;
			}
		}
	}
	ret = mlx5_vdpa_create_indirect_mkey(priv);
	if (ret) {
		DRV_LOG(ERR, "Failed to create indirect mkey .");
		goto error;
	}
	return 0;
error:
	mlx5_vdpa_mem_dereg(priv);
	rte_errno = -ret;
	return ret;
}

int
mlx5_vdpa_register_mr(struct mlx5_vdpa_priv *priv, uint32_t idx)
{
	struct rte_vhost_memory *mem = priv->vmem_info.vmem;
	struct mlx5_vdpa_query_mr *mrs =
		(struct mlx5_vdpa_query_mr *)priv->mrs;
	struct mlx5_vdpa_query_mr *entry;
	struct rte_vhost_mem_region *reg;
	int ret;

	reg = &mem->regions[idx];
	entry = &mrs[idx];
	entry->mr = mlx5_glue->reg_mr_iova
				      (priv->cdev->pd,
				       (void *)(uintptr_t)(reg->host_user_addr),
				       reg->size, reg->guest_phys_addr,
				       IBV_ACCESS_LOCAL_WRITE);
	if (!entry->mr) {
		DRV_LOG(ERR, "Failed to create direct Mkey.");
		ret = -rte_errno;
		return ret;
	}
	entry->is_indirect = 0;
	return 0;
}
