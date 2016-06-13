/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <xen/xen-compat.h>
#if __XEN_LATEST_INTERFACE_VERSION__ < 0x00040200
#include <xs.h>
#else
#include <xenstore.h>
#endif
#include <xen/sys/gntalloc.h>

#include <rte_common.h>
#include <rte_string_fns.h>
#include <rte_malloc.h>

#include "rte_xen_lib.h"

/*
 * The grant node format in xenstore for vring/mpool is:
 * 0_rx_vring_gref = "gref1#, gref2#, gref3#"
 * 0_mempool_gref  = "gref1#, gref2#, gref3#"
 * each gref# is a grant reference for a shared page.
 * In each shared page, we store the grant_node_item items.
 */
struct grant_node_item {
	uint32_t gref;
	uint32_t pfn;
} __attribute__((packed));

/* fd for xen_gntalloc driver, used to allocate grant pages*/
int gntalloc_fd = -1;

/* xenstore path for local domain, now it is '/local/domain/domid/' */
static char *dompath = NULL;
/* handle to xenstore read/write operations */
static struct xs_handle *xs = NULL;
/* flag to indicate if xenstore cleanup is required */
static bool is_xenstore_cleaned_up;

/*
 * Reserve a virtual address space.
 * On success, returns the pointer. On failure, returns NULL.
 */
void *
get_xen_virtual(size_t size, size_t page_sz)
{
	void *addr;
	uintptr_t aligned_addr;

	addr = mmap(NULL, size + page_sz, PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		RTE_LOG(ERR, PMD, "failed get a virtual area\n");
		return NULL;
	}

	aligned_addr = RTE_ALIGN_CEIL((uintptr_t)addr, page_sz);
	addr = (void *)(aligned_addr);

	return addr;
}

/*
 * Get the physical address for virtual memory starting at va.
 */
int
get_phys_map(void *va, phys_addr_t pa[], uint32_t pg_num, uint32_t pg_sz)
{
	int32_t fd, rc = 0;
	uint32_t i, nb;
	off_t ofs;

	ofs = (uintptr_t)va / pg_sz * sizeof(*pa);
	nb = pg_num * sizeof(*pa);

	if ((fd = open(PAGEMAP_FNAME, O_RDONLY)) < 0 ||
			(rc = pread(fd, pa, nb, ofs)) < 0 ||
			(rc -= nb) != 0) {
		RTE_LOG(ERR, PMD, "%s: failed read of %u bytes from \'%s\' "
			"at offset %lu, error code: %d\n",
			__func__, nb, PAGEMAP_FNAME, (unsigned long)ofs, errno);
		rc = ENOENT;
	}

	close(fd);
	for (i = 0; i != pg_num; i++)
		pa[i] = (pa[i] & PAGEMAP_PFN_MASK) * pg_sz;

	return rc;
}

int
gntalloc_open(void)
{
	gntalloc_fd = open(XEN_GNTALLOC_FNAME, O_RDWR);
	return (gntalloc_fd != -1) ? 0 : -1;
}

void
gntalloc_close(void)
{
	if (gntalloc_fd != -1)
		close(gntalloc_fd);
	gntalloc_fd = -1;
}

void *
gntalloc(size_t size, uint32_t *gref, uint64_t *start_index)
{
	int page_size = getpagesize();
	uint32_t i, pg_num;
	void *va;
	int rv;
	struct ioctl_gntalloc_alloc_gref *arg;
	struct ioctl_gntalloc_dealloc_gref arg_d;

	if (size % page_size) {
		RTE_LOG(ERR, PMD, "%s: %zu isn't multiple of page size\n",
			__func__, size);
		return NULL;
	}

	pg_num = size / page_size;
	arg = malloc(sizeof(*arg) + (pg_num - 1) * sizeof(uint32_t));
	if (arg == NULL)
		return NULL;
	arg->domid = DOM0_DOMID;
	arg->flags = GNTALLOC_FLAG_WRITABLE;
	arg->count = pg_num;

	rv = ioctl(gntalloc_fd, IOCTL_GNTALLOC_ALLOC_GREF, arg);
	if (rv) {
		RTE_LOG(ERR, PMD, "%s: ioctl error\n", __func__);
		free(arg);
		return NULL;
	}

	va = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, gntalloc_fd, arg->index);
	if (va == MAP_FAILED) {
		RTE_LOG(ERR, PMD, "%s: mmap failed\n", __func__);
		arg_d.count = pg_num;
		arg_d.index = arg->index;
		ioctl(gntalloc_fd, IOCTL_GNTALLOC_DEALLOC_GREF, arg_d);
		free(arg);
		return NULL;
	}

	if (gref) {
		for (i = 0; i < pg_num; i++) {
			gref[i] = arg->gref_ids[i];
		}
	}
	if (start_index)
		*start_index = arg->index;

	free(arg);

	return va;
}

int
grefwatch_from_alloc(uint32_t *gref, void **pptr)
{
	int rv;
	void *ptr;
	int pg_size = getpagesize();
	struct ioctl_gntalloc_alloc_gref arg = {
		.domid = DOM0_DOMID,
		.flags = GNTALLOC_FLAG_WRITABLE,
		.count = 1
	};
	struct ioctl_gntalloc_dealloc_gref arg_d;
	struct ioctl_gntalloc_unmap_notify notify = {
		.action = UNMAP_NOTIFY_CLEAR_BYTE
	};

	rv = ioctl(gntalloc_fd, IOCTL_GNTALLOC_ALLOC_GREF, &arg);
	if (rv) {
		RTE_LOG(ERR, PMD, "%s: ioctl error\n", __func__);
		return -1;
	}

	ptr = (void *)mmap(NULL, pg_size, PROT_READ|PROT_WRITE, MAP_SHARED, gntalloc_fd, arg.index);
	arg_d.index = arg.index;
	arg_d.count = 1;
	if (ptr == MAP_FAILED) {
		RTE_LOG(ERR, PMD, "%s: mmap failed\n", __func__);
		ioctl(gntalloc_fd, IOCTL_GNTALLOC_DEALLOC_GREF, &arg_d);
		return -1;
	}
	if (pptr)
		*pptr = ptr;
	if (gref)
		*gref = arg.gref_ids[0];

	notify.index = arg.index;
	rv = ioctl(gntalloc_fd, IOCTL_GNTALLOC_SET_UNMAP_NOTIFY, &notify);
	if (rv) {
		RTE_LOG(ERR, PMD, "%s: unmap notify failed\n", __func__);
		munmap(ptr, pg_size);
		ioctl(gntalloc_fd, IOCTL_GNTALLOC_DEALLOC_GREF, &arg_d);
		return -1;
	}

	return 0;
}

void
gntfree(void *va, size_t sz, uint64_t start_index)
{
	struct ioctl_gntalloc_dealloc_gref arg_d;

	if (va && sz) {
		munmap(va, sz);
		arg_d.count = sz / getpagesize();
		arg_d.index = start_index;
		ioctl(gntalloc_fd, IOCTL_GNTALLOC_DEALLOC_GREF, &arg_d);
	}
}

static int
xenstore_cleanup(void)
{
	char store_path[PATH_MAX] = {0};

	if (snprintf(store_path, sizeof(store_path),
		"%s%s", dompath, DPDK_XENSTORE_NODE) == -1)
		return -1;

	if (xs_rm(xs, XBT_NULL, store_path) == false) {
		RTE_LOG(ERR, PMD, "%s: failed cleanup node\n", __func__);
		return -1;
	}

	return 0;
}

int
xenstore_init(void)
{
	unsigned int len, domid;
	char *buf;
	char *end;

	xs = xs_domain_open();
	if (xs == NULL) {
		RTE_LOG(ERR, PMD,"%s: xs_domain_open failed\n", __func__);
		return -1;
	}
	buf = xs_read(xs, XBT_NULL, "domid", &len);
	if (buf == NULL) {
		RTE_LOG(ERR, PMD, "%s: failed read domid\n", __func__);
		return -1;
	}
	errno = 0;
	domid = strtoul(buf, &end, 0);
	if (errno != 0 || end == NULL || end == buf ||  domid == 0)
		return -1;

	RTE_LOG(INFO, PMD, "retrieved dom ID = %d\n", domid);

	dompath = xs_get_domain_path(xs, domid);
	if (dompath == NULL)
		return -1;

	xs_transaction_start(xs); /* When to stop transaction */

	if (is_xenstore_cleaned_up == 0) {
		if (xenstore_cleanup())
			return -1;
		is_xenstore_cleaned_up = 1;
	}

	return 0;
}

int
xenstore_uninit(void)
{
	xs_close(xs);

	if (is_xenstore_cleaned_up == 0) {
		if (xenstore_cleanup())
			return -1;
		is_xenstore_cleaned_up = 1;
	}
	free(dompath);
	dompath = NULL;

	return 0;
}

int
xenstore_write(const char *key_str, const char *val_str)
{
	char grant_path[PATH_MAX];
	int rv, len;

	if (xs == NULL) {
		RTE_LOG(ERR, PMD, "%s: xenstore init failed\n", __func__);
		return -1;
	}
	rv = snprintf(grant_path, sizeof(grant_path), "%s%s", dompath, key_str);
	if (rv == -1) {
		RTE_LOG(ERR, PMD, "%s: snprintf %s %s failed\n",
			__func__, dompath, key_str);
		return -1;
	}
	len = strnlen(val_str, PATH_MAX);

	if (xs_write(xs, XBT_NULL, grant_path, val_str, len) == false) {
		RTE_LOG(ERR, PMD, "%s: xs_write failed\n", __func__);
		return -1;
	}

	return 0;
}

int
grant_node_create(uint32_t pg_num, uint32_t *gref_arr, phys_addr_t *pa_arr, char *val_str, size_t str_size)
{
	uint64_t start_index;
	int pg_size;
	uint32_t pg_shift;
	void *ptr = NULL;
	uint32_t count, entries_per_pg;
	uint32_t i, j = 0, k = 0;
	uint32_t *gref_tmp;
	int first = 1;
	char tmp_str[PATH_MAX] = {0};
	int rv = -1;

	pg_size = getpagesize();
	if (rte_is_power_of_2(pg_size) == 0) {
		return -1;
	}
	pg_shift = rte_bsf32(pg_size);
	if (pg_size % sizeof(struct grant_node_item)) {
		RTE_LOG(ERR, PMD, "pg_size isn't a multiple of grant node item\n");
		return -1;
	}

	entries_per_pg = pg_size / sizeof(struct grant_node_item);
	count  = (pg_num +  entries_per_pg - 1 ) / entries_per_pg;
	gref_tmp = malloc(count * sizeof(uint32_t));
	if (gref_tmp == NULL)
		return -1;
	ptr = gntalloc(pg_size * count, gref_tmp, &start_index);
	if (ptr == NULL) {
		RTE_LOG(ERR, PMD, "%s: gntalloc error of %d pages\n", __func__, count);
		free(gref_tmp);
		return -1;
	}

	while (j < pg_num) {
		if (first) {
			rv = snprintf(val_str, str_size, "%u", gref_tmp[k]);
			first = 0;
		} else {
			snprintf(tmp_str, PATH_MAX, "%s", val_str);
			rv = snprintf(val_str, str_size, "%s,%u", tmp_str, gref_tmp[k]);
		}
		k++;
		if (rv == -1)
			break;

		for (i = 0; i < entries_per_pg && j < pg_num ; i++) {
			((struct grant_node_item *)ptr)->gref = gref_arr[j];
			((struct grant_node_item *)ptr)->pfn =  pa_arr[j] >> pg_shift;
			ptr = RTE_PTR_ADD(ptr, sizeof(struct grant_node_item));
			j++;
		}
	}
	if (rv == -1) {
		gntfree(ptr, pg_size * count, start_index);
	} else
		rv = 0;
	free(gref_tmp);
	return rv;
}


int
grant_gntalloc_mbuf_pool(struct rte_mempool *mpool, uint32_t pg_num, uint32_t *gref_arr, phys_addr_t *pa_arr, int mempool_idx)
{
	char key_str[PATH_MAX] = {0};
	char val_str[PATH_MAX] = {0};
	void *mempool_obj_va;

	if (grant_node_create(pg_num, gref_arr, pa_arr, val_str, sizeof(val_str))) {
		return -1;
	}

	if (snprintf(key_str, sizeof(key_str),
		DPDK_XENSTORE_PATH"%d"MEMPOOL_XENSTORE_STR, mempool_idx) == -1)
		return -1;
	if (xenstore_write(key_str, val_str) == -1)
		return -1;

	if (snprintf(key_str, sizeof(key_str),
		DPDK_XENSTORE_PATH"%d"MEMPOOL_VA_XENSTORE_STR, mempool_idx) == -1)
		return -1;
	if (mpool->nb_mem_chunks != 1) {
		RTE_LOG(ERR, PMD,
			"mempool with more than 1 chunk is not supported\n");
		return -1;
	}
	mempool_obj_va = STAILQ_FIRST(&mpool->mem_list)->addr;
	if (snprintf(val_str, sizeof(val_str), "%"PRIxPTR,
			(uintptr_t)mempool_obj_va) == -1)
		return -1;
	if (xenstore_write(key_str, val_str) == -1)
		return -1;

	return 0;
}
