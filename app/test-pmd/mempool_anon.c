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

#include <sys/types.h>
#include <sys/stat.h>
#include "mempool_osdep.h"
#include <rte_errno.h>

#ifdef RTE_EXEC_ENV_LINUXAPP

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>


#define	PAGEMAP_FNAME		"/proc/self/pagemap"

/*
 * the pfn (page frame number) are bits 0-54 (see pagemap.txt in linux
 * Documentation).
 */
#define	PAGEMAP_PFN_BITS	54
#define	PAGEMAP_PFN_MASK	RTE_LEN2MASK(PAGEMAP_PFN_BITS, phys_addr_t)


static int
get_phys_map(void *va, phys_addr_t pa[], uint32_t pg_num, uint32_t pg_sz)
{
	int32_t fd, rc;
	uint32_t i, nb;
	off_t ofs;

	ofs = (uintptr_t)va / pg_sz * sizeof(*pa);
	nb = pg_num * sizeof(*pa);

	if ((fd = open(PAGEMAP_FNAME, O_RDONLY)) < 0)
		return (ENOENT);

	if ((rc = pread(fd, pa, nb, ofs)) < 0 || (rc -= nb) != 0) {

		RTE_LOG(ERR, USER1, "failed read of %u bytes from \'%s\' "
			"at offset %zu, error code: %d\n",
			nb, PAGEMAP_FNAME, (size_t)ofs, errno);
		rc = ENOENT;
	}

	close(fd);

	for (i = 0; i != pg_num; i++)
		pa[i] = (pa[i] & PAGEMAP_PFN_MASK) * pg_sz;

	return (rc);
}

struct rte_mempool *
mempool_anon_create(const char *name, unsigned elt_num, unsigned elt_size,
		   unsigned cache_size, unsigned private_data_size,
		   rte_mempool_ctor_t *mp_init, void *mp_init_arg,
		   rte_mempool_obj_ctor_t *obj_init, void *obj_init_arg,
		   int socket_id, unsigned flags)
{
	struct rte_mempool *mp;
	phys_addr_t *pa;
	char *va, *uv;
	uint32_t n, pg_num, pg_shift, pg_sz, total_size;
	size_t sz;
	ssize_t usz;
	int32_t rc;

	rc = ENOMEM;
	mp = NULL;

	pg_sz = getpagesize();
	if (rte_is_power_of_2(pg_sz) == 0) {
		rte_errno = EINVAL;
		return (mp);
	}

	pg_shift = rte_bsf32(pg_sz);

	total_size = rte_mempool_calc_obj_size(elt_size, flags, NULL);

	/* calc max memory size and max number of pages needed. */
	sz = rte_mempool_xmem_size(elt_num, total_size, pg_shift);
	pg_num = sz >> pg_shift;

	/* get chunk of virtually continuos memory.*/
	if ((va = mmap(NULL, sz, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_ANONYMOUS | MAP_LOCKED,
			-1, 0)) == MAP_FAILED) {
		RTE_LOG(ERR, USER1, "%s(%s) failed mmap of %zu bytes, "
			"error code: %d\n",
			__func__, name, sz, errno);
		rte_errno = rc;
		return (mp);
	}

	/* extract physical mappings of the allocated memory. */
	if ((pa = calloc(pg_num, sizeof (*pa))) != NULL &&
			(rc = get_phys_map(va, pa, pg_num, pg_sz)) == 0) {

		/*
		 * Check that allocated size is big enough to hold elt_num
		 * objects and a calcualte how many bytes are actually required.
		 */

		if ((usz = rte_mempool_xmem_usage(va, elt_num, total_size, pa,
				pg_num, pg_shift)) < 0) {

			n = -usz;
			rc = ENOENT;
			RTE_LOG(ERR, USER1, "%s(%s) only %u objects from %u "
				"requested can  be created over "
				"mmaped region %p of %zu bytes\n",
				__func__, name, n, elt_num, va, sz);
		} else {

			/* unmap unused pages if any */
			if ((size_t)usz < sz) {

				uv = va + usz;
				usz = sz - usz;

				RTE_LOG(INFO, USER1,
					"%s(%s): unmap unused %zu of %zu "
					"mmaped bytes @%p\n",
					__func__, name, (size_t)usz, sz, uv);
				munmap(uv, usz);
				sz -= usz;
				pg_num = sz >> pg_shift;
			}

			if ((mp = rte_mempool_xmem_create(name, elt_num,
					elt_size, cache_size, private_data_size,
					mp_init, mp_init_arg,
					obj_init, obj_init_arg,
					socket_id, flags, va, pa, pg_num,
					pg_shift)) != NULL)

				RTE_VERIFY(elt_num == mp->size);
		}
	}

	if (mp == NULL) {
		munmap(va, sz);
		rte_errno = rc;
	}

	free(pa);
	return (mp);
}

#else /* RTE_EXEC_ENV_LINUXAPP */


struct rte_mempool *
mempool_anon_create(__rte_unused const char *name,
	__rte_unused unsigned elt_num, __rte_unused unsigned elt_size,
	__rte_unused unsigned cache_size,
	__rte_unused unsigned private_data_size,
	__rte_unused rte_mempool_ctor_t *mp_init,
	__rte_unused void *mp_init_arg,
	__rte_unused rte_mempool_obj_ctor_t *obj_init,
	__rte_unused void *obj_init_arg,
	__rte_unused int socket_id, __rte_unused unsigned flags)
{
	rte_errno = ENOTSUP;
	return (NULL);
}

#endif /* RTE_EXEC_ENV_LINUXAPP */
