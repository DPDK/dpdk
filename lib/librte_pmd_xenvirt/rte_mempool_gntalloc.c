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
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <string.h>
#include <xen/sys/gntalloc.h>

#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_memory.h>
#include <rte_errno.h>

#include "rte_xen_lib.h"
#include "rte_eth_xenvirt.h"

struct _gntarr {
	uint32_t gref;
	phys_addr_t pa;
	uint64_t index;
	void *va;
};

struct _mempool_gntalloc_info {
	struct rte_mempool *mp;
	uint32_t pg_num;
	uint32_t *gref_arr;
	phys_addr_t *pa_arr;
	void *va;
	uint32_t mempool_idx;
	uint64_t start_index;
};


static rte_atomic32_t global_xenvirt_mempool_idx = RTE_ATOMIC32_INIT(-1);

static int
compare(const void *p1, const void *p2)
{
	return ((const struct _gntarr *)p1)->pa  - ((const struct _gntarr *)p2)->pa;
}


static struct _mempool_gntalloc_info
_create_mempool(const char *name, unsigned elt_num, unsigned elt_size,
		   unsigned cache_size, unsigned private_data_size,
		   rte_mempool_ctor_t *mp_init, void *mp_init_arg,
		   rte_mempool_obj_ctor_t *obj_init, void *obj_init_arg,
		   int socket_id, unsigned flags)
{
	struct _mempool_gntalloc_info mgi;
	struct rte_mempool *mp = NULL;
	struct rte_mempool_objsz  objsz;
	uint32_t pg_num, rpg_num, pg_shift, pg_sz;
	char *va, *orig_va, *uv; /* uv: from which, the pages could be freed */
	ssize_t sz, usz; /* usz: unused size */
	/*
	 * for each page allocated through xen_gntalloc driver,
	 * gref_arr:stores grant references,
	 * pa_arr: stores physical address,
	 * gnt_arr: stores all meta dat
	 */
	uint32_t *gref_arr = NULL;
	phys_addr_t *pa_arr = NULL;
	struct _gntarr *gnt_arr = NULL;
	/* start index of the grant referances, used for dealloc*/
	uint64_t start_index;
	uint32_t i, j;
	int rv = 0;
	struct ioctl_gntalloc_dealloc_gref arg;

	mgi.mp = NULL;
	va = orig_va = uv = NULL;
	pg_num = rpg_num = 0;
	sz = 0;

	pg_sz = getpagesize();
	if (rte_is_power_of_2(pg_sz) == 0) {
		goto out;
	}
	pg_shift = rte_bsf32(pg_sz);

	rte_mempool_calc_obj_size(elt_size, flags, &objsz);
	sz = rte_mempool_xmem_size(elt_num, objsz.total_size, pg_shift);
	pg_num = sz >> pg_shift;

	pa_arr = calloc(pg_num, sizeof(pa_arr[0]));
	gref_arr = calloc(pg_num, sizeof(gref_arr[0]));
	gnt_arr  = calloc(pg_num, sizeof(gnt_arr[0]));
	if ((gnt_arr == NULL) || (gref_arr == NULL) || (pa_arr == NULL))
		goto out;

	/* grant index is continuous in ascending order */
	orig_va = gntalloc(sz, gref_arr, &start_index);
	if (orig_va == NULL)
		goto out;

	get_phys_map(orig_va, pa_arr, pg_num, pg_sz);
	for (i = 0; i < pg_num; i++) {
		gnt_arr[i].index = start_index + i * pg_sz;
		gnt_arr[i].gref = gref_arr[i];
		gnt_arr[i].pa = pa_arr[i];
		gnt_arr[i].va  = RTE_PTR_ADD(orig_va, i * pg_sz);
	}
	qsort(gnt_arr, pg_num, sizeof(struct _gntarr), compare);

	va = get_xen_virtual(sz, pg_sz);
	if (va == NULL) {
		goto out;
	}

	/*
	 * map one by one, as index isn't continuous now.
	 * pg_num VMAs, doesn't linux has a limitation on this?
	 */
	for (i = 0; i < pg_num; i++) {
	/* update gref_arr and pa_arr after sort */
		gref_arr[i] = gnt_arr[i].gref;
		pa_arr[i]   = gnt_arr[i].pa;
		gnt_arr[i].va = mmap(va + i * pg_sz, pg_sz, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_FIXED, gntalloc_fd, gnt_arr[i].index);
		if ((gnt_arr[i].va == MAP_FAILED) || (gnt_arr[i].va != (va + i * pg_sz))) {
			RTE_LOG(ERR, PMD, "failed to map %d pages\n", i);
			goto mmap_failed;
		}
	}

	/*
	 * Check that allocated size is big enough to hold elt_num
	 * objects and a calcualte how many bytes are actually required.
	 */
	usz = rte_mempool_xmem_usage(va, elt_num, objsz.total_size, pa_arr, pg_num, pg_shift);
	if (usz < 0) {
		mp = NULL;
		i = pg_num;
		goto mmap_failed;
	} else {
		/* unmap unused pages if any */
		uv = RTE_PTR_ADD(va, usz);
		if ((usz = va + sz - uv) > 0) {

			RTE_LOG(ERR, PMD,
				"%s(%s): unmap unused %zu of %zu "
				"mmaped bytes @%p orig:%p\n",
				__func__, name, usz, sz, uv, va);
			munmap(uv, usz);
			i = (sz - usz) / pg_sz;
			for (; i < pg_num; i++) {
				arg.count = 1;
				arg.index = gnt_arr[i].index;
				rv = ioctl(gntalloc_fd, IOCTL_GNTALLOC_DEALLOC_GREF, &arg);
				if (rv) {
					/* shouldn't fail here */
					RTE_LOG(ERR, PMD, "va=%p pa=%p index=%p %s\n",
						gnt_arr[i].va,
						(void *)gnt_arr[i].pa,
						(void *)arg.index, strerror(errno));
					rte_panic("gntdealloc failed when freeing pages\n");
				}
			}

			rpg_num = (sz - usz) >> pg_shift;
		} else
			rpg_num = pg_num;

		mp = rte_mempool_xmem_create(name, elt_num, elt_size,
				cache_size, private_data_size,
				mp_init, mp_init_arg,
				obj_init, obj_init_arg,
				socket_id, flags, va, pa_arr, rpg_num, pg_shift);

		RTE_VERIFY(elt_num == mp->size);
	}
	mgi.mp = mp;
	mgi.pg_num = rpg_num;
	mgi.gref_arr = gref_arr;
	mgi.pa_arr = pa_arr;
	if (mp)
		mgi.mempool_idx = rte_atomic32_add_return(&global_xenvirt_mempool_idx, 1);
	mgi.start_index = start_index;
	mgi.va = va;

	if (mp == NULL) {
		i = pg_num;
		goto mmap_failed;
	}

/*
 * unmap only, without deallocate grant reference.
 * unused pages have already been unmaped,
 * unmap twice will fail, but it is safe.
 */
mmap_failed:
	for (j = 0; j < i; j++) {
		if (gnt_arr[i].va)
			munmap(gnt_arr[i].va, pg_sz);
	}
out:
	if (gnt_arr)
		free(gnt_arr);
	if (orig_va)
		munmap(orig_va, sz);
	if (mp == NULL) {
		if (gref_arr)
			free(gref_arr);
		if (pa_arr)
			free(pa_arr);

		/* some gref has already been de-allocated from the list in the driver,
		 * so dealloc one by one, and it is safe to deallocate twice
		 */
		if (orig_va) {
			for (i = 0; i < pg_num; i++) {
				arg.index = start_index + i * pg_sz;
				rv = ioctl(gntalloc_fd, IOCTL_GNTALLOC_DEALLOC_GREF, arg);
			}
		}
	}
	return mgi;
}

struct rte_mempool *
rte_mempool_gntalloc_create(const char *name, unsigned elt_num, unsigned elt_size,
		   unsigned cache_size, unsigned private_data_size,
		   rte_mempool_ctor_t *mp_init, void *mp_init_arg,
		   rte_mempool_obj_ctor_t *obj_init, void *obj_init_arg,
		   int socket_id, unsigned flags)
{
	int rv;
	uint32_t i;
	struct _mempool_gntalloc_info mgi;
	struct ioctl_gntalloc_dealloc_gref arg;
	int pg_sz = getpagesize();

	mgi = _create_mempool(name, elt_num, elt_size,
			cache_size, private_data_size,
			mp_init, mp_init_arg,
			obj_init, obj_init_arg,
			socket_id, flags);
	if (mgi.mp) {
		rv = grant_gntalloc_mbuf_pool(mgi.mp,
			mgi.pg_num,
			mgi.gref_arr,
			mgi.pa_arr,
			mgi.mempool_idx);
		free(mgi.gref_arr);
		free(mgi.pa_arr);
		if (rv == 0)
			return mgi.mp;
		/*
		 * in _create_mempool, unused pages have already been unmapped, deallocagted
		 * unmap and dealloc the remained ones here.
		 */
		munmap(mgi.va, pg_sz * mgi.pg_num);
		for (i = 0; i < mgi.pg_num; i++) {
			arg.index = mgi.start_index + i * pg_sz;
			rv = ioctl(gntalloc_fd, IOCTL_GNTALLOC_DEALLOC_GREF, arg);
		}
		return NULL;
	}
	return NULL;



}
