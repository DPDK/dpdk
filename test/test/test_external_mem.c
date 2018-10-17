/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <rte_common.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_ring.h>
#include <rte_string_fns.h>

#include "test.h"

#define EXTERNAL_MEM_SZ (RTE_PGSIZE_4K << 10) /* 4M of data */

static int
test_invalid_param(void *addr, size_t len, size_t pgsz, rte_iova_t *iova,
		int n_pages)
{
	static const char * const names[] = {
		NULL, /* NULL name */
		"",   /* empty name */
		"this heap name is definitely way too long to be valid"
	};
	const char *valid_name = "valid heap name";
	unsigned int i;

	/* check invalid name handling */
	for (i = 0; i < RTE_DIM(names); i++) {
		const char *name = names[i];

		/* these calls may fail for other reasons, so check errno */
		if (rte_malloc_heap_create(name) >= 0 || rte_errno != EINVAL) {
			printf("%s():%i: Created heap with invalid name\n",
					__func__, __LINE__);
			goto fail;
		}

		if (rte_malloc_heap_destroy(name) >= 0 || rte_errno != EINVAL) {
			printf("%s():%i: Destroyed heap with invalid name\n",
					__func__, __LINE__);
			goto fail;
		}

		if (rte_malloc_heap_get_socket(name) >= 0 ||
				rte_errno != EINVAL) {
			printf("%s():%i: Found socket for heap with invalid name\n",
					__func__, __LINE__);
			goto fail;
		}

		if (rte_malloc_heap_memory_add(name, addr, len,
				NULL, 0, pgsz) >= 0 || rte_errno != EINVAL) {
			printf("%s():%i: Added memory to heap with invalid name\n",
					__func__, __LINE__);
			goto fail;
		}
		if (rte_malloc_heap_memory_remove(name, addr, len) >= 0 ||
				rte_errno != EINVAL) {
			printf("%s():%i: Removed memory from heap with invalid name\n",
					__func__, __LINE__);
			goto fail;
		}

		if (rte_malloc_heap_memory_attach(name, addr, len) >= 0 ||
				rte_errno != EINVAL) {
			printf("%s():%i: Attached memory to heap with invalid name\n",
				__func__, __LINE__);
			goto fail;
		}
		if (rte_malloc_heap_memory_detach(name, addr, len) >= 0 ||
				rte_errno != EINVAL) {
			printf("%s():%i: Detached memory from heap with invalid name\n",
				__func__, __LINE__);
			goto fail;
		}
	}

	/* do same as above, but with a valid heap name */

	/* skip create call */
	if (rte_malloc_heap_destroy(valid_name) >= 0 || rte_errno != ENOENT) {
		printf("%s():%i: Destroyed heap with invalid name\n",
			__func__, __LINE__);
		goto fail;
	}
	if (rte_malloc_heap_get_socket(valid_name) >= 0 ||
			rte_errno != ENOENT) {
		printf("%s():%i: Found socket for heap with invalid name\n",
				__func__, __LINE__);
		goto fail;
	}

	/* these calls may fail for other reasons, so check errno */
	if (rte_malloc_heap_memory_add(valid_name, addr, len,
			NULL, 0, pgsz) >= 0 || rte_errno != ENOENT) {
		printf("%s():%i: Added memory to non-existent heap\n",
			__func__, __LINE__);
		goto fail;
	}
	if (rte_malloc_heap_memory_remove(valid_name, addr, len) >= 0 ||
			rte_errno != ENOENT) {
		printf("%s():%i: Removed memory from non-existent heap\n",
			__func__, __LINE__);
		goto fail;
	}

	if (rte_malloc_heap_memory_attach(valid_name, addr, len) >= 0 ||
			rte_errno != ENOENT) {
		printf("%s():%i: Attached memory to non-existent heap\n",
			__func__, __LINE__);
		goto fail;
	}
	if (rte_malloc_heap_memory_detach(valid_name, addr, len) >= 0 ||
			rte_errno != ENOENT) {
		printf("%s():%i: Detached memory from non-existent heap\n",
			__func__, __LINE__);
		goto fail;
	}

	/* create a valid heap but test other invalid parameters */
	if (rte_malloc_heap_create(valid_name) != 0) {
		printf("%s():%i: Failed to create valid heap\n",
			__func__, __LINE__);
		goto fail;
	}

	/* zero length */
	if (rte_malloc_heap_memory_add(valid_name, addr, 0,
			NULL, 0, pgsz) >= 0 || rte_errno != EINVAL) {
		printf("%s():%i: Added memory with invalid parameters\n",
			__func__, __LINE__);
		goto fail;
	}

	if (rte_malloc_heap_memory_remove(valid_name, addr, 0) >= 0 ||
			rte_errno != EINVAL) {
		printf("%s():%i: Removed memory with invalid parameters\n",
			__func__, __LINE__);
		goto fail;
	}

	if (rte_malloc_heap_memory_attach(valid_name, addr, 0) >= 0 ||
			rte_errno != EINVAL) {
		printf("%s():%i: Attached memory with invalid parameters\n",
			__func__, __LINE__);
		goto fail;
	}
	if (rte_malloc_heap_memory_detach(valid_name, addr, 0) >= 0 ||
			rte_errno != EINVAL) {
		printf("%s():%i: Detached memory with invalid parameters\n",
			__func__, __LINE__);
		goto fail;
	}

	/* zero address */
	if (rte_malloc_heap_memory_add(valid_name, NULL, len,
			NULL, 0, pgsz) >= 0 || rte_errno != EINVAL) {
		printf("%s():%i: Added memory with invalid parameters\n",
			__func__, __LINE__);
		goto fail;
	}

	if (rte_malloc_heap_memory_remove(valid_name, NULL, len) >= 0 ||
			rte_errno != EINVAL) {
		printf("%s():%i: Removed memory with invalid parameters\n",
			__func__, __LINE__);
		goto fail;
	}

	if (rte_malloc_heap_memory_attach(valid_name, NULL, len) >= 0 ||
			rte_errno != EINVAL) {
		printf("%s():%i: Attached memory with invalid parameters\n",
			__func__, __LINE__);
		goto fail;
	}
	if (rte_malloc_heap_memory_detach(valid_name, NULL, len) >= 0 ||
			rte_errno != EINVAL) {
		printf("%s():%i: Detached memory with invalid parameters\n",
			__func__, __LINE__);
		goto fail;
	}

	/* wrong page count */
	if (rte_malloc_heap_memory_add(valid_name, addr, len,
			iova, 0, pgsz) >= 0 || rte_errno != EINVAL) {
		printf("%s():%i: Added memory with invalid parameters\n",
			__func__, __LINE__);
		goto fail;
	}
	if (rte_malloc_heap_memory_add(valid_name, addr, len,
			iova, n_pages - 1, pgsz) >= 0 || rte_errno != EINVAL) {
		printf("%s():%i: Added memory with invalid parameters\n",
			__func__, __LINE__);
		goto fail;
	}
	if (rte_malloc_heap_memory_add(valid_name, addr, len,
			iova, n_pages + 1, pgsz) >= 0 || rte_errno != EINVAL) {
		printf("%s():%i: Added memory with invalid parameters\n",
			__func__, __LINE__);
		goto fail;
	}

	/* tests passed, destroy heap */
	if (rte_malloc_heap_destroy(valid_name) != 0) {
		printf("%s():%i: Failed to destroy valid heap\n",
			__func__, __LINE__);
		goto fail;
	}
	return 0;
fail:
	rte_malloc_heap_destroy(valid_name);
	return -1;
}

static int
test_basic(void *addr, size_t len, size_t pgsz, rte_iova_t *iova, int n_pages)
{
	const char *heap_name = "heap";
	void *ptr = NULL;
	int socket_id, i;
	const struct rte_memzone *mz = NULL;

	/* create heap */
	if (rte_malloc_heap_create(heap_name) != 0) {
		printf("%s():%i: Failed to create malloc heap\n",
			__func__, __LINE__);
		goto fail;
	}

	/* get socket ID corresponding to this heap */
	socket_id = rte_malloc_heap_get_socket(heap_name);
	if (socket_id < 0) {
		printf("%s():%i: cannot find socket for external heap\n",
			__func__, __LINE__);
		goto fail;
	}

	/* heap is empty, so any allocation should fail */
	ptr = rte_malloc_socket("EXTMEM", 64, 0, socket_id);
	if (ptr != NULL) {
		printf("%s():%i: Allocated from empty heap\n", __func__,
			__LINE__);
		goto fail;
	}

	/* add memory to heap */
	if (rte_malloc_heap_memory_add(heap_name, addr, len,
			iova, n_pages, pgsz) != 0) {
		printf("%s():%i: Failed to add memory to heap\n",
			__func__, __LINE__);
		goto fail;
	}

	/* check that we can get this memory from EAL now */
	for (i = 0; i < n_pages; i++) {
		const struct rte_memseg *ms;
		void *cur = RTE_PTR_ADD(addr, pgsz * i);

		ms = rte_mem_virt2memseg(cur, NULL);
		if (ms == NULL) {
			printf("%s():%i: Failed to retrieve memseg for external mem\n",
				__func__, __LINE__);
			goto fail;
		}
		if (ms->addr != cur) {
			printf("%s():%i: VA mismatch\n", __func__, __LINE__);
			goto fail;
		}
		if (ms->iova != iova[i]) {
			printf("%s():%i: IOVA mismatch\n", __func__, __LINE__);
			goto fail;
		}
	}

	/* allocate - this now should succeed */
	ptr = rte_malloc_socket("EXTMEM", 64, 0, socket_id);
	if (ptr == NULL) {
		printf("%s():%i: Failed to allocate from external heap\n",
			__func__, __LINE__);
		goto fail;
	}

	/* check if address is in expected range */
	if (ptr < addr || ptr >= RTE_PTR_ADD(addr, len)) {
		printf("%s():%i: Allocated from unexpected address space\n",
			__func__, __LINE__);
		goto fail;
	}

	/* we've allocated something - removing memory should fail */
	if (rte_malloc_heap_memory_remove(heap_name, addr, len) >= 0 ||
			rte_errno != EBUSY) {
		printf("%s():%i: Removing memory succeeded when memory is not free\n",
			__func__, __LINE__);
		goto fail;
	}
	if (rte_malloc_heap_destroy(heap_name) >= 0 || rte_errno != EBUSY) {
		printf("%s():%i: Destroying heap succeeded when memory is not free\n",
			__func__, __LINE__);
		goto fail;
	}

	/* try allocating an IOVA-contiguous memzone - this should succeed
	 * because we've set up a contiguous IOVA table.
	 */
	mz = rte_memzone_reserve("heap_test", pgsz * 2, socket_id,
			RTE_MEMZONE_IOVA_CONTIG);
	if (mz == NULL) {
		printf("%s():%i: Failed to reserve memzone\n",
			__func__, __LINE__);
		goto fail;
	}

	rte_malloc_dump_stats(stdout, NULL);
	rte_malloc_dump_heaps(stdout);

	/* free memory - removing it should now succeed */
	rte_free(ptr);
	ptr = NULL;

	rte_memzone_free(mz);
	mz = NULL;

	if (rte_malloc_heap_memory_remove(heap_name, addr, len) != 0) {
		printf("%s():%i: Removing memory from heap failed\n",
			__func__, __LINE__);
		goto fail;
	}
	if (rte_malloc_heap_destroy(heap_name) != 0) {
		printf("%s():%i: Destroying heap failed\n",
			__func__, __LINE__);
		goto fail;
	}

	return 0;
fail:
	rte_memzone_free(mz);
	rte_free(ptr);
	/* even if something failed, attempt to clean up */
	rte_malloc_heap_memory_remove(heap_name, addr, len);
	rte_malloc_heap_destroy(heap_name);

	return -1;
}

/* we need to test attach/detach in secondary processes. */
static int
test_external_mem(void)
{
	size_t len = EXTERNAL_MEM_SZ;
	size_t pgsz = RTE_PGSIZE_4K;
	rte_iova_t iova[len / pgsz];
	void *addr;
	int ret, n_pages;
	int i;

	/* create external memory area */
	n_pages = RTE_DIM(iova);
	addr = mmap(NULL, len, PROT_WRITE | PROT_READ,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (addr == MAP_FAILED) {
		printf("%s():%i: Failed to create dummy memory area\n",
			__func__, __LINE__);
		return -1;
	}
	for (i = 0; i < n_pages; i++) {
		/* arbitrary IOVA */
		rte_iova_t tmp = 0x100000000 + i * pgsz;
		iova[i] = tmp;
	}

	ret = test_invalid_param(addr, len, pgsz, iova, n_pages);
	ret |= test_basic(addr, len, pgsz, iova, n_pages);

	munmap(addr, len);

	return ret;
}

REGISTER_TEST_COMMAND(external_mem_autotest, test_external_mem);
