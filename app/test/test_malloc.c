/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_random.h>
#include <rte_string_fns.h>

#include "test.h"

#define N 10000


static int
is_mem_on_socket(int32_t socket);

static int32_t
addr_to_socket(void *addr);

/*
 * Malloc
 * ======
 *
 * Allocate some dynamic memory from heap (3 areas). Check that areas
 * don't overlap and that alignment constraints match. This test is
 * done many times on different lcores simultaneously.
 */

/* Test if memory overlaps: return 1 if true, or 0 if false. */
static int
is_memory_overlap(void *p1, size_t len1, void *p2, size_t len2)
{
	unsigned long ptr1 = (unsigned long)p1;
	unsigned long ptr2 = (unsigned long)p2;

	if (ptr2 >= ptr1 && (ptr2 - ptr1) < len1)
		return 1;
	else if (ptr2 < ptr1 && (ptr1 - ptr2) < len2)
		return 1;
	return 0;
}

static int
is_aligned(void *p, int align)
{
	unsigned long addr = (unsigned long)p;
	unsigned mask = align - 1;

	if (addr & mask)
		return 0;
	return 1;
}

static int
test_align_overlap_per_lcore(__rte_unused void *arg)
{
	const unsigned align1 = 8,
			align2 = 64,
			align3 = 2048;
	unsigned i,j;
	void *p1 = NULL, *p2 = NULL, *p3 = NULL;
	int ret = 0;

	for (i = 0; i < N; i++) {
		p1 = rte_zmalloc("dummy", 1000, align1);
		if (!p1){
			printf("rte_zmalloc returned NULL (i=%u)\n", i);
			ret = -1;
			break;
		}
		for(j = 0; j < 1000 ; j++) {
			if( *(char *)p1 != 0) {
				printf("rte_zmalloc didn't zero the allocated memory\n");
				ret = -1;
			}
		}
		p2 = rte_malloc("dummy", 1000, align2);
		if (!p2){
			printf("rte_malloc returned NULL (i=%u)\n", i);
			ret = -1;
			rte_free(p1);
			break;
		}
		p3 = rte_malloc("dummy", 1000, align3);
		if (!p3){
			printf("rte_malloc returned NULL (i=%u)\n", i);
			ret = -1;
			rte_free(p1);
			rte_free(p2);
			break;
		}
		if (is_memory_overlap(p1, 1000, p2, 1000)) {
			printf("p1 and p2 overlaps\n");
			ret = -1;
		}
		if (is_memory_overlap(p2, 1000, p3, 1000)) {
			printf("p2 and p3 overlaps\n");
			ret = -1;
		}
		if (is_memory_overlap(p1, 1000, p3, 1000)) {
			printf("p1 and p3 overlaps\n");
			ret = -1;
		}
		if (!is_aligned(p1, align1)) {
			printf("p1 is not aligned\n");
			ret = -1;
		}
		if (!is_aligned(p2, align2)) {
			printf("p2 is not aligned\n");
			ret = -1;
		}
		if (!is_aligned(p3, align3)) {
			printf("p3 is not aligned\n");
			ret = -1;
		}
		rte_free(p1);
		rte_free(p2);
		rte_free(p3);
	}
	rte_malloc_dump_stats(stdout, "dummy");

	return ret;
}

static int
test_reordered_free_per_lcore(__rte_unused void *arg)
{
	const unsigned align1 = 8,
			align2 = 64,
			align3 = 2048;
	unsigned i,j;
	void *p1, *p2, *p3;
	int ret = 0;

	for (i = 0; i < 30; i++) {
		p1 = rte_zmalloc("dummy", 1000, align1);
		if (!p1){
			printf("rte_zmalloc returned NULL (i=%u)\n", i);
			ret = -1;
			break;
		}
		for(j = 0; j < 1000 ; j++) {
			if( *(char *)p1 != 0) {
				printf("rte_zmalloc didn't zero the allocated memory\n");
				ret = -1;
			}
		}
		/* use calloc to allocate 1000 16-byte items this time */
		p2 = rte_calloc("dummy", 1000, 16, align2);
		/* for third request use regular malloc again */
		p3 = rte_malloc("dummy", 1000, align3);
		if (!p2 || !p3){
			printf("rte_malloc returned NULL (i=%u)\n", i);
			ret = -1;
			break;
		}
		if (is_memory_overlap(p1, 1000, p2, 1000)) {
			printf("p1 and p2 overlaps\n");
			ret = -1;
		}
		if (is_memory_overlap(p2, 1000, p3, 1000)) {
			printf("p2 and p3 overlaps\n");
			ret = -1;
		}
		if (is_memory_overlap(p1, 1000, p3, 1000)) {
			printf("p1 and p3 overlaps\n");
			ret = -1;
		}
		if (!is_aligned(p1, align1)) {
			printf("p1 is not aligned\n");
			ret = -1;
		}
		if (!is_aligned(p2, align2)) {
			printf("p2 is not aligned\n");
			ret = -1;
		}
		if (!is_aligned(p3, align3)) {
			printf("p3 is not aligned\n");
			ret = -1;
		}
		/* try freeing in every possible order */
		switch (i%6){
		case 0:
			rte_free(p1);
			rte_free(p2);
			rte_free(p3);
			break;
		case 1:
			rte_free(p1);
			rte_free(p3);
			rte_free(p2);
			break;
		case 2:
			rte_free(p2);
			rte_free(p1);
			rte_free(p3);
			break;
		case 3:
			rte_free(p2);
			rte_free(p3);
			rte_free(p1);
			break;
		case 4:
			rte_free(p3);
			rte_free(p1);
			rte_free(p2);
			break;
		case 5:
			rte_free(p3);
			rte_free(p2);
			rte_free(p1);
			break;
		}
	}
	rte_malloc_dump_stats(stdout, "dummy");

	return ret;
}

/* test function inside the malloc lib*/
static int
test_str_to_size(void)
{
	struct {
		const char *str;
		uint64_t value;
	} test_values[] =
	{{ "5G", (uint64_t)5 * 1024 * 1024 *1024 },
			{"0x20g", (uint64_t)0x20 * 1024 * 1024 *1024},
			{"10M", 10 * 1024 * 1024},
			{"050m", 050 * 1024 * 1024},
			{"8K", 8 * 1024},
			{"15k", 15 * 1024},
			{"0200", 0200},
			{"0x103", 0x103},
			{"432", 432},
			{"-1", 0}, /* negative values return 0 */
			{"  -2", 0},
			{"  -3MB", 0},
			{"18446744073709551616", 0} /* ULLONG_MAX + 1 == out of range*/
	};
	unsigned i;
	for (i = 0; i < RTE_DIM(test_values); i++)
		if (rte_str_to_size(test_values[i].str) != test_values[i].value)
			return -1;
	return 0;
}

static int
test_multi_alloc_statistics(void)
{
	int socket = 0;
	struct rte_malloc_socket_stats pre_stats, post_stats ,first_stats, second_stats;
	size_t size = 2048;
	int align = 1024;
	int overhead = 0;

	/* Dynamically calculate the overhead by allocating one cacheline and
	 * then comparing what was allocated from the heap.
	 */
	rte_malloc_get_socket_stats(socket, &pre_stats);

	void *dummy = rte_malloc_socket(NULL, RTE_CACHE_LINE_SIZE, 0, socket);
	if (dummy == NULL)
		return -1;

	rte_malloc_get_socket_stats(socket, &post_stats);

	/* after subtracting cache line, remainder is overhead */
	overhead = post_stats.heap_allocsz_bytes - pre_stats.heap_allocsz_bytes;
	overhead -= RTE_CACHE_LINE_SIZE;

	rte_free(dummy);

	/* Now start the real tests */
	rte_malloc_get_socket_stats(socket, &pre_stats);

	void *p1 = rte_malloc_socket("stats", size , align, socket);
	if (!p1)
		return -1;
	rte_free(p1);
	rte_malloc_dump_stats(stdout, "stats");

	rte_malloc_get_socket_stats(socket,&post_stats);
	/* Check statistics reported are correct */
	/* All post stats should be equal to pre stats after alloc freed */
	if ((post_stats.heap_totalsz_bytes != pre_stats.heap_totalsz_bytes) &&
			(post_stats.heap_freesz_bytes!=pre_stats.heap_freesz_bytes) &&
			(post_stats.heap_allocsz_bytes!=pre_stats.heap_allocsz_bytes)&&
			(post_stats.alloc_count!=pre_stats.alloc_count)&&
			(post_stats.free_count!=pre_stats.free_count)) {
		printf("Malloc statistics are incorrect - freed alloc\n");
		return -1;
	}
	/* Check two consecutive allocations */
	size = 1024;
	align = 0;
	rte_malloc_get_socket_stats(socket,&pre_stats);
	void *p2 = rte_malloc_socket("add", size ,align, socket);
	if (!p2)
		return -1;
	rte_malloc_get_socket_stats(socket,&first_stats);

	void *p3 = rte_malloc_socket("add2", size,align, socket);
	if (!p3)
		return -1;

	rte_malloc_get_socket_stats(socket,&second_stats);

	rte_free(p2);
	rte_free(p3);

	/* After freeing both allocations check stats return to original */
	rte_malloc_get_socket_stats(socket, &post_stats);

	if(second_stats.heap_totalsz_bytes != first_stats.heap_totalsz_bytes) {
		printf("Incorrect heap statistics: Total size \n");
		return -1;
	}
	/* Check allocated size is equal to two additions plus overhead */
	if(second_stats.heap_allocsz_bytes !=
			size + overhead + first_stats.heap_allocsz_bytes) {
		printf("Incorrect heap statistics: Allocated size \n");
		return -1;
	}
	/* Check that allocation count increments correctly i.e. +1 */
	if (second_stats.alloc_count != first_stats.alloc_count + 1) {
		printf("Incorrect heap statistics: Allocated count \n");
		return -1;
	}

	if (second_stats.free_count != first_stats.free_count){
		printf("Incorrect heap statistics: Free count \n");
		return -1;
	}

	/* Make sure that we didn't touch our greatest chunk: 2 * 11M)  */
	if (post_stats.greatest_free_size != pre_stats.greatest_free_size) {
		printf("Incorrect heap statistics: Greatest free size \n");
		return -1;
	}
	/* Free size must equal the original free size minus the new allocation*/
	if (first_stats.heap_freesz_bytes <= second_stats.heap_freesz_bytes) {
		printf("Incorrect heap statistics: Free size \n");
		return -1;
	}

	if ((post_stats.heap_totalsz_bytes != pre_stats.heap_totalsz_bytes) &&
			(post_stats.heap_freesz_bytes!=pre_stats.heap_freesz_bytes) &&
			(post_stats.heap_allocsz_bytes!=pre_stats.heap_allocsz_bytes)&&
			(post_stats.alloc_count!=pre_stats.alloc_count)&&
			(post_stats.free_count!=pre_stats.free_count)) {
		printf("Malloc statistics are incorrect - freed alloc\n");
		return -1;
	}
	return 0;
}

static int
test_realloc_socket(int socket)
{
	const char hello_str[] = "Hello, world!";
	const unsigned size1 = 1024;
	const unsigned size2 = size1 + 1024;
	const unsigned size3 = size2;
	const unsigned size4 = size3 + 1024;

	/* test data is the same even if element is moved*/
	char *ptr1 = rte_zmalloc_socket(
			NULL, size1, RTE_CACHE_LINE_SIZE, socket);
	if (!ptr1){
		printf("NULL pointer returned from rte_zmalloc\n");
		return -1;
	}
	strlcpy(ptr1, hello_str, size1);
	char *ptr2 = rte_realloc_socket(
			ptr1, size2, RTE_CACHE_LINE_SIZE, socket);
	if (!ptr2){
		rte_free(ptr1);
		printf("NULL pointer returned from rte_realloc\n");
		return -1;
	}
	if (ptr1 == ptr2){
		printf("unexpected - ptr1 == ptr2\n");
	}
	if (strcmp(ptr2, hello_str) != 0){
		printf("Error - lost data from pointed area\n");
		rte_free(ptr2);
		return -1;
	}
	unsigned i;
	for (i = strnlen(hello_str, sizeof(hello_str)); i < size1; i++)
		if (ptr2[i] != 0){
			printf("Bad data in realloc\n");
			rte_free(ptr2);
			return -1;
		}
	/* now allocate third element, free the second
	 * and resize third. It should not move. (ptr1 is now invalid)
	 */
	char *ptr3 = rte_zmalloc_socket(
			NULL, size3, RTE_CACHE_LINE_SIZE, socket);
	if (!ptr3){
		printf("NULL pointer returned from rte_zmalloc\n");
		rte_free(ptr2);
		return -1;
	}
	for (i = 0; i < size3; i++)
		if (ptr3[i] != 0){
			printf("Bad data in zmalloc\n");
			rte_free(ptr3);
			rte_free(ptr2);
			return -1;
		}
	rte_free(ptr2);
	/* first resize to half the size of the freed block */
	char *ptr4 = rte_realloc_socket(
			ptr3, size4, RTE_CACHE_LINE_SIZE, socket);
	if (!ptr4){
		printf("NULL pointer returned from rte_realloc\n");
		rte_free(ptr3);
		return -1;
	}
	if (ptr3 != ptr4){
		printf("Unexpected - ptr4 != ptr3\n");
		rte_free(ptr4);
		return -1;
	}
	/* now resize again to the full size of the freed block */
	ptr4 = rte_realloc_socket(ptr3, size3 + size2 + size1,
			RTE_CACHE_LINE_SIZE, socket);
	if (ptr3 != ptr4){
		printf("Unexpected - ptr4 != ptr3 on second resize\n");
		rte_free(ptr4);
		return -1;
	}
	rte_free(ptr4);

	/* now try a resize to a smaller size, see if it works */
	const unsigned size5 = 1024;
	const unsigned size6 = size5 / 2;
	char *ptr5 = rte_malloc_socket(
			NULL, size5, RTE_CACHE_LINE_SIZE, socket);
	if (!ptr5){
		printf("NULL pointer returned from rte_malloc\n");
		return -1;
	}
	char *ptr6 = rte_realloc_socket(
			ptr5, size6, RTE_CACHE_LINE_SIZE, socket);
	if (!ptr6){
		printf("NULL pointer returned from rte_realloc\n");
		rte_free(ptr5);
		return -1;
	}
	if (ptr5 != ptr6){
		printf("Error, resizing to a smaller size moved data\n");
		rte_free(ptr6);
		return -1;
	}
	rte_free(ptr6);

	/* check for behaviour changing alignment */
	const unsigned size7 = 1024;
	const unsigned orig_align = RTE_CACHE_LINE_SIZE;
	unsigned new_align = RTE_CACHE_LINE_SIZE * 2;
	char *ptr7 = rte_malloc_socket(NULL, size7, orig_align, socket);
	if (!ptr7){
		printf("NULL pointer returned from rte_malloc\n");
		return -1;
	}
	/* calc an alignment we don't already have */
	while(RTE_PTR_ALIGN(ptr7, new_align) == ptr7)
		new_align *= 2;
	char *ptr8 = rte_realloc_socket(ptr7, size7, new_align, socket);
	if (!ptr8){
		printf("NULL pointer returned from rte_realloc\n");
		rte_free(ptr7);
		return -1;
	}
	if (RTE_PTR_ALIGN(ptr8, new_align) != ptr8){
		printf("Failure to re-align data\n");
		rte_free(ptr8);
		return -1;
	}
	rte_free(ptr8);

	/* test behaviour when there is a free block after current one,
	 * but its not big enough
	 */
	unsigned size9 = 1024, size10 = 1024;
	unsigned size11 = size9 + size10 + 256;
	char *ptr9 = rte_malloc_socket(
			NULL, size9, RTE_CACHE_LINE_SIZE, socket);
	if (!ptr9){
		printf("NULL pointer returned from rte_malloc\n");
		return -1;
	}
	char *ptr10 = rte_malloc_socket(
			NULL, size10, RTE_CACHE_LINE_SIZE, socket);
	if (!ptr10){
		printf("NULL pointer returned from rte_malloc\n");
		return -1;
	}
	rte_free(ptr9);
	char *ptr11 = rte_realloc_socket(
			ptr10, size11, RTE_CACHE_LINE_SIZE, socket);
	if (!ptr11){
		printf("NULL pointer returned from rte_realloc\n");
		rte_free(ptr10);
		return -1;
	}
	if (ptr11 == ptr10){
		printf("Error, unexpected that realloc has not created new buffer\n");
		rte_free(ptr11);
		return -1;
	}
	rte_free(ptr11);

	/* check we don't crash if we pass null to realloc
	 * We should get a malloc of the size requested*/
	const size_t size12 = 1024;
	size_t size12_check;
	char *ptr12 = rte_realloc_socket(
			NULL, size12, RTE_CACHE_LINE_SIZE, socket);
	if (!ptr12){
		printf("NULL pointer returned from rte_realloc\n");
		return -1;
	}
	if (rte_malloc_validate(ptr12, &size12_check) < 0 ||
			size12_check != size12){
		rte_free(ptr12);
		return -1;
	}
	rte_free(ptr12);

	/* do the same, but for regular memory */
	ptr12 = rte_realloc(NULL, size12, RTE_CACHE_LINE_SIZE);
	if (!ptr12) {
		printf("NULL pointer returned from rte_realloc\n");
		return -1;
	}
	if (rte_malloc_validate(ptr12, &size12_check) < 0 ||
			size12_check != size12) {
		rte_free(ptr12);
		return -1;
	}
	rte_free(ptr12);

	return 0;
}

static int
test_realloc_numa(void)
{
	/* check realloc_socket part */
	int32_t socket_count = 0, socket_allocated, socket;
	void *ptr1, *ptr2;
	int ret = -1;
	size_t size = 1024;

	ptr1 = NULL;
	for (socket = 0; socket < RTE_MAX_NUMA_NODES; socket++) {
		if (is_mem_on_socket(socket)) {
			int j = 2;

			socket_count++;
			while (j--) {
				/* j == 1 -> resizing */
				ptr2 = rte_realloc_socket(ptr1, size,
							  RTE_CACHE_LINE_SIZE,
							  socket);
				if (ptr2 == NULL) {
					printf("NULL pointer returned from rte_realloc_socket\n");
					goto end;
				}

				ptr1 = ptr2;
				socket_allocated = addr_to_socket(ptr2);
				if (socket_allocated != socket) {
					printf("Requested socket (%d) doesn't mach allocated one (%d)\n",
					       socket, socket_allocated);
					goto end;
				}
				size += RTE_CACHE_LINE_SIZE;
			}
		}
	}

	/* Print warnign if only a single socket, but don't fail the test */
	if (socket_count < 2)
		printf("WARNING: realloc_socket test needs memory on multiple sockets!\n");

	ret = 0;
end:
	rte_free(ptr1);
	return ret;
}

static int
test_realloc(void)
{
	const char *heap_name = "realloc_heap";
	int realloc_heap_socket;
	unsigned int mem_sz = 1U << 13; /* 8K */
	unsigned int page_sz = sysconf(_SC_PAGESIZE);
	void *mem;
	int ret;

	/* page size may be bigger than total mem size, so adjust */
	mem_sz = RTE_MAX(mem_sz, page_sz);

	/*
	 * the realloc tests depend on specific layout of underlying memory, so
	 * to prevent accidental failures to do fragmented main heap, we will
	 * do all of our tests on an artificially created memory.
	 */
	if (rte_malloc_heap_create(heap_name) != 0) {
		printf("Failed to create external heap\n");
		ret = -1;
		goto end;
	}
	realloc_heap_socket = rte_malloc_heap_get_socket(heap_name);

	mem = mmap(NULL, mem_sz, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mem == MAP_FAILED) {
		printf("Failed to allocate memory for external heap\n");
		ret = -1;
		goto heap_destroy;
	}

	if (rte_malloc_heap_memory_add(
			heap_name, mem, mem_sz, NULL, 0, page_sz) != 0) {
		printf("Failed to add memory to external heap\n");
		ret = -1;
		goto mem_free;
	}

	/* run the socket-bound tests */
	ret = test_realloc_socket(realloc_heap_socket);
	if (ret != 0)
		goto mem_remove;

	/* now, run the NUMA node tests */
	ret = test_realloc_numa();

mem_remove:
	rte_malloc_heap_memory_remove(heap_name, mem, mem_sz);
mem_free:
	munmap(mem, mem_sz);
heap_destroy:
	rte_malloc_heap_destroy(heap_name);
end:
	return ret;
}

static int
test_random_alloc_free(void *_ __rte_unused)
{
	struct mem_list {
		struct mem_list *next;
		char data[0];
	} *list_head = NULL;
	unsigned i;
	unsigned count = 0;

	rte_srand((unsigned)rte_rdtsc());

	for (i = 0; i < N; i++){
		unsigned free_mem = 0;
		size_t allocated_size;
		while (!free_mem){
			const unsigned mem_size = sizeof(struct mem_list) + \
					rte_rand() % (64 * 1024);
			const unsigned align = 1 << (rte_rand() % 12); /* up to 4k alignment */
			struct mem_list *entry = rte_malloc(NULL,
					mem_size, align);
			if (entry == NULL)
				return -1;
			if (RTE_PTR_ALIGN(entry, align)!= entry)
				return -1;
			if (rte_malloc_validate(entry, &allocated_size) == -1
					|| allocated_size < mem_size)
				return -1;
			memset(entry->data, rte_lcore_id(),
					mem_size - sizeof(*entry));
			entry->next = list_head;
			if (rte_malloc_validate(entry, NULL) == -1)
				return -1;
			list_head = entry;

			count++;
			/* switch to freeing the memory with a 20% probability */
			free_mem = ((rte_rand() % 10) >= 8);
		}
		while (list_head){
			struct mem_list *entry = list_head;
			list_head = list_head->next;
			rte_free(entry);
		}
	}
	printf("Lcore %u allocated/freed %u blocks\n", rte_lcore_id(), count);
	return 0;
}

#define err_return() do { \
	printf("%s: %d - Error\n", __func__, __LINE__); \
	goto err_return; \
} while (0)

static int
test_rte_malloc_validate(void)
{
	const size_t request_size = 1024;
	size_t allocated_size;
	char *data_ptr = rte_malloc(NULL, request_size, RTE_CACHE_LINE_SIZE);
#ifdef RTE_MALLOC_DEBUG
	int retval;
	char *over_write_vals = NULL;
#endif

	if (data_ptr == NULL) {
		printf("%s: %d - Allocation error\n", __func__, __LINE__);
		return -1;
	}

	/* check that a null input returns -1 */
	if (rte_malloc_validate(NULL, NULL) != -1)
		err_return();

	/* check that we get ok on a valid pointer */
	if (rte_malloc_validate(data_ptr, &allocated_size) < 0)
		err_return();

	/* check that the returned size is ok */
	if (allocated_size < request_size)
		err_return();

#ifdef RTE_MALLOC_DEBUG

	/****** change the header to be bad */
	char save_buf[64];
	over_write_vals = (char *)((uintptr_t)data_ptr - sizeof(save_buf));
	/* first save the data as a backup before overwriting it */
	memcpy(save_buf, over_write_vals, sizeof(save_buf));
	memset(over_write_vals, 1, sizeof(save_buf));
	/* then run validate */
	retval = rte_malloc_validate(data_ptr, NULL);
	/* finally restore the data again */
	memcpy(over_write_vals, save_buf, sizeof(save_buf));
	/* check we previously had an error */
	if (retval != -1)
		err_return();

	/* check all ok again */
	if (rte_malloc_validate(data_ptr, &allocated_size) < 0)
		err_return();

	/**** change the trailer to be bad */
	over_write_vals = (char *)((uintptr_t)data_ptr + allocated_size);
	/* first save the data as a backup before overwriting it */
	memcpy(save_buf, over_write_vals, sizeof(save_buf));
	memset(over_write_vals, 1, sizeof(save_buf));
	/* then run validate */
	retval = rte_malloc_validate(data_ptr, NULL);
	/* finally restore the data again */
	memcpy(over_write_vals, save_buf, sizeof(save_buf));
	if (retval != -1)
		err_return();

	/* check all ok again */
	if (rte_malloc_validate(data_ptr, &allocated_size) < 0)
		err_return();
#endif

	rte_free(data_ptr);
	return 0;

err_return:
	/*clean up */
	rte_free(data_ptr);
	return -1;
}

static int
test_zero_aligned_alloc(void)
{
	char *p1 = rte_malloc(NULL,1024, 0);
	if (!p1)
		goto err_return;
	if (!rte_is_aligned(p1, RTE_CACHE_LINE_SIZE))
		goto err_return;
	rte_free(p1);
	return 0;

err_return:
	/*clean up */
	if (p1) rte_free(p1);
	return -1;
}

static int
test_malloc_bad_params(void)
{
	const char *type = NULL;
	size_t size = 0;
	unsigned align = RTE_CACHE_LINE_SIZE;

	/* rte_malloc expected to return null with inappropriate size */
	char *bad_ptr = rte_malloc(type, size, align);
	if (bad_ptr != NULL)
		goto err_return;

	/* rte_realloc expected to return null with inappropriate size */
	bad_ptr = rte_realloc(NULL, size, align);
	if (bad_ptr != NULL)
		goto err_return;

	/* rte_malloc expected to return null with inappropriate alignment */
	align = 17;
	size = 1024;

	bad_ptr = rte_malloc(type, size, align);
	if (bad_ptr != NULL)
		goto err_return;

	/* rte_realloc expected to return null with inappropriate alignment */
	bad_ptr = rte_realloc(NULL, size, align);
	if (bad_ptr != NULL)
		goto err_return;

	/* rte_malloc expected to return null with size will cause overflow */
	align = RTE_CACHE_LINE_SIZE;
	size = (size_t)-8;

	bad_ptr = rte_malloc(type, size, align);
	if (bad_ptr != NULL)
		goto err_return;

	bad_ptr = rte_realloc(NULL, size, align);
	if (bad_ptr != NULL)
		goto err_return;

	return 0;

err_return:
	/* clean up pointer */
	if (bad_ptr)
		rte_free(bad_ptr);
	return -1;
}

static int
check_socket_mem(const struct rte_memseg_list *msl, void *arg)
{
	int32_t *socket = arg;

	if (msl->external)
		return 0;

	return *socket == msl->socket_id;
}

/* Check if memory is available on a specific socket */
static int
is_mem_on_socket(int32_t socket)
{
	return rte_memseg_list_walk(check_socket_mem, &socket);
}


/*
 * Find what socket a memory address is on. Only works for addresses within
 * memsegs, not heap or stack...
 */
static int32_t
addr_to_socket(void * addr)
{
	const struct rte_memseg *ms = rte_mem_virt2memseg(addr, NULL);
	return ms == NULL ? -1 : ms->socket_id;

}

/* Test using rte_[c|m|zm]alloc_socket() on a specific socket */
static int
test_alloc_single_socket(int32_t socket)
{
	const char *type = NULL;
	const size_t size = 10;
	const unsigned align = 0;
	char *mem = NULL;
	int32_t desired_socket = (socket == SOCKET_ID_ANY) ?
			(int32_t)rte_socket_id() : socket;

	/* Test rte_calloc_socket() */
	mem = rte_calloc_socket(type, size, sizeof(char), align, socket);
	if (mem == NULL)
		return -1;
	if (addr_to_socket(mem) != desired_socket) {
		rte_free(mem);
		return -1;
	}
	rte_free(mem);

	/* Test rte_malloc_socket() */
	mem = rte_malloc_socket(type, size, align, socket);
	if (mem == NULL)
		return -1;
	if (addr_to_socket(mem) != desired_socket) {
		return -1;
	}
	rte_free(mem);

	/* Test rte_zmalloc_socket() */
	mem = rte_zmalloc_socket(type, size, align, socket);
	if (mem == NULL)
		return -1;
	if (addr_to_socket(mem) != desired_socket) {
		rte_free(mem);
		return -1;
	}
	rte_free(mem);

	return 0;
}

static int
test_alloc_socket(void)
{
	unsigned socket_count = 0;
	unsigned i;

	if (test_alloc_single_socket(SOCKET_ID_ANY) < 0)
		return -1;

	for (i = 0; i < RTE_MAX_NUMA_NODES; i++) {
		if (is_mem_on_socket(i)) {
			socket_count++;
			if (test_alloc_single_socket(i) < 0) {
				printf("Fail: rte_malloc_socket(..., %u) did not succeed\n",
						i);
				return -1;
			}
		}
		else {
			if (test_alloc_single_socket(i) == 0) {
				printf("Fail: rte_malloc_socket(..., %u) succeeded\n",
						i);
				return -1;
			}
		}
	}

	/* Print warnign if only a single socket, but don't fail the test */
	if (socket_count < 2) {
		printf("WARNING: alloc_socket test needs memory on multiple sockets!\n");
	}

	return 0;
}

static int
test_malloc(void)
{
	unsigned lcore_id;
	int ret = 0;

	if (test_str_to_size() < 0){
		printf("test_str_to_size() failed\n");
		return -1;
	}
	else printf("test_str_to_size() passed\n");

	if (test_zero_aligned_alloc() < 0){
		printf("test_zero_aligned_alloc() failed\n");
		return -1;
	}
	else printf("test_zero_aligned_alloc() passed\n");

	if (test_malloc_bad_params() < 0){
		printf("test_malloc_bad_params() failed\n");
		return -1;
	}
	else printf("test_malloc_bad_params() passed\n");

	if (test_realloc() < 0){
		printf("test_realloc() failed\n");
		return -1;
	}
	else printf("test_realloc() passed\n");

	/*----------------------------*/
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(test_align_overlap_per_lcore, NULL, lcore_id);
	}

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			ret = -1;
	}
	if (ret < 0){
		printf("test_align_overlap_per_lcore() failed\n");
		return ret;
	}
	else printf("test_align_overlap_per_lcore() passed\n");

	/*----------------------------*/
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(test_reordered_free_per_lcore, NULL, lcore_id);
	}

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			ret = -1;
	}
	if (ret < 0){
		printf("test_reordered_free_per_lcore() failed\n");
		return ret;
	}
	else printf("test_reordered_free_per_lcore() passed\n");

	/*----------------------------*/
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(test_random_alloc_free, NULL, lcore_id);
	}

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			ret = -1;
	}
	if (ret < 0){
		printf("test_random_alloc_free() failed\n");
		return ret;
	}
	else printf("test_random_alloc_free() passed\n");

	/*----------------------------*/
	ret = test_rte_malloc_validate();
	if (ret < 0){
		printf("test_rte_malloc_validate() failed\n");
		return ret;
	}
	else printf("test_rte_malloc_validate() passed\n");

	ret = test_alloc_socket();
	if (ret < 0){
		printf("test_alloc_socket() failed\n");
		return ret;
	}
	else printf("test_alloc_socket() passed\n");

	ret = test_multi_alloc_statistics();
	if (ret < 0) {
		printf("test_multi_alloc_statistics() failed\n");
		return ret;
	}
	else
		printf("test_multi_alloc_statistics() passed\n");

	return 0;
}

REGISTER_TEST_COMMAND(malloc_autotest, test_malloc);
