/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#include <rte_lcore.h>
#include <rte_fbarray.h>
#include <rte_memzone.h>
#include <rte_memory.h>
#include <rte_eal_memconfig.h>

#include "eal_private.h"
#include "eal_internal_cfg.h"
#include "eal_memalloc.h"

bool
eal_memalloc_is_contig(const struct rte_memseg_list *msl, void *start,
		size_t len)
{
	void *end, *aligned_start, *aligned_end;
	size_t pgsz = (size_t)msl->page_sz;
	const struct rte_memseg *ms;

	/* for IOVA_VA, it's always contiguous */
	if (rte_eal_iova_mode() == RTE_IOVA_VA)
		return true;

	/* for legacy memory, it's always contiguous */
	if (internal_config.legacy_mem)
		return true;

	end = RTE_PTR_ADD(start, len);

	/* for nohuge, we check pagemap, otherwise check memseg */
	if (!rte_eal_has_hugepages()) {
		rte_iova_t cur, expected;

		aligned_start = RTE_PTR_ALIGN_FLOOR(start, pgsz);
		aligned_end = RTE_PTR_ALIGN_CEIL(end, pgsz);

		/* if start and end are on the same page, bail out early */
		if (RTE_PTR_DIFF(aligned_end, aligned_start) == pgsz)
			return true;

		/* skip first iteration */
		cur = rte_mem_virt2iova(aligned_start);
		expected = cur + pgsz;
		aligned_start = RTE_PTR_ADD(aligned_start, pgsz);

		while (aligned_start < aligned_end) {
			cur = rte_mem_virt2iova(aligned_start);
			if (cur != expected)
				return false;
			aligned_start = RTE_PTR_ADD(aligned_start, pgsz);
			expected += pgsz;
		}
	} else {
		int start_seg, end_seg, cur_seg;
		rte_iova_t cur, expected;

		aligned_start = RTE_PTR_ALIGN_FLOOR(start, pgsz);
		aligned_end = RTE_PTR_ALIGN_CEIL(end, pgsz);

		start_seg = RTE_PTR_DIFF(aligned_start, msl->base_va) /
				pgsz;
		end_seg = RTE_PTR_DIFF(aligned_end, msl->base_va) /
				pgsz;

		/* if start and end are on the same page, bail out early */
		if (RTE_PTR_DIFF(aligned_end, aligned_start) == pgsz)
			return true;

		/* skip first iteration */
		ms = rte_fbarray_get(&msl->memseg_arr, start_seg);
		cur = ms->iova;
		expected = cur + pgsz;

		/* if we can't access IOVA addresses, assume non-contiguous */
		if (cur == RTE_BAD_IOVA)
			return false;

		for (cur_seg = start_seg + 1; cur_seg < end_seg;
				cur_seg++, expected += pgsz) {
			ms = rte_fbarray_get(&msl->memseg_arr, cur_seg);

			if (ms->iova != expected)
				return false;
		}
	}
	return true;
}
