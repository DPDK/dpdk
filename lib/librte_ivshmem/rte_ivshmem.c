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
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>

#include <rte_eal_memconfig.h>
#include <rte_memory.h>
#include <rte_ivshmem.h>
#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_spinlock.h>
#include <rte_common.h>
#include <rte_malloc.h>

#include "rte_ivshmem.h"

#define IVSHMEM_CONFIG_FILE_FMT "/var/run/.dpdk_ivshmem_metadata_%s"
#define IVSHMEM_QEMU_CMD_LINE_HEADER_FMT "-device ivshmem,size=%" PRIu64 "M,shm=fd%s"
#define IVSHMEM_QEMU_CMD_FD_FMT ":%s:0x%" PRIx64 ":0x%" PRIx64
#define IVSHMEM_QEMU_CMDLINE_BUFSIZE 1024
#define IVSHMEM_MAX_PAGES (1 << 12)
#define adjacent(x,y) (((x).phys_addr+(x).len)==(y).phys_addr)
#define METADATA_SIZE_ALIGNED \
	(RTE_ALIGN_CEIL(sizeof(struct rte_ivshmem_metadata),pagesz))

#define GET_PAGEMAP_ADDR(in,addr,dlm,err)    \
{                                      \
	char *end;                         \
	errno = 0;                         \
	addr = strtoull((in), &end, 16);   \
	if (errno != 0 || *end != (dlm)) { \
		RTE_LOG(ERR, EAL, err);        \
		goto error;                    \
	}                                  \
	(in) = end + 1;                    \
}

static int pagesz;

struct memseg_cache_entry {
	char filepath[PATH_MAX];
	uint64_t offset;
	uint64_t len;
};

struct ivshmem_config {
	struct rte_ivshmem_metadata * metadata;
	struct memseg_cache_entry memseg_cache[IVSHMEM_MAX_PAGES];
		/**< account for multiple files per segment case */
	struct flock lock;
	rte_spinlock_t sl;
};

static struct ivshmem_config
ivshmem_global_config[RTE_LIBRTE_IVSHMEM_MAX_METADATA_FILES];

static rte_spinlock_t global_cfg_sl;

static struct ivshmem_config *
get_config_by_name(const char * name)
{
	struct rte_ivshmem_metadata * config;
	unsigned i;

	for (i = 0; i < RTE_DIM(ivshmem_global_config); i++) {
		config = ivshmem_global_config[i].metadata;
		if (config == NULL)
			return NULL;
		if (strncmp(name, config->name, IVSHMEM_NAME_LEN) == 0)
			return &ivshmem_global_config[i];
	}

	return NULL;
}

static int
overlap(const struct rte_memzone * s1, const struct rte_memzone * s2)
{
	uint64_t start1, end1, start2, end2;

	start1 = s1->addr_64;
	end1 = s1->addr_64 + s1->len;
	start2 = s2->addr_64;
	end2 = s2->addr_64 + s2->len;

	if (start1 >= start2 && start1 < end2)
		return 1;
	if (start2 >= start1 && start2 < end1)
		return 1;

	return 0;
}

static struct rte_memzone *
get_memzone_by_addr(const void * addr)
{
	struct rte_memzone * tmp, * mz;
	struct rte_mem_config * mcfg;
	int i;

	mcfg = rte_eal_get_configuration()->mem_config;
	mz = NULL;

	/* find memzone for the ring */
	for (i = 0; i < RTE_MAX_MEMZONE; i++) {
		tmp = &mcfg->memzone[i];

		if (tmp->addr_64 == (uint64_t) addr) {
			mz = tmp;
			break;
		}
	}

	return mz;
}

static int
entry_compare(const void * a, const void * b)
{
	const struct rte_ivshmem_metadata_entry * e1 =
			(const struct rte_ivshmem_metadata_entry*) a;
	const struct rte_ivshmem_metadata_entry * e2 =
			(const struct rte_ivshmem_metadata_entry*) b;

	/* move unallocated zones to the end */
	if (e1->mz.addr == NULL && e2->mz.addr == NULL)
		return 0;
	if (e1->mz.addr == 0)
		return 1;
	if (e2->mz.addr == 0)
		return -1;

	return e1->mz.phys_addr > e2->mz.phys_addr;
}

/* fills hugepage cache entry for a given start virt_addr */
static int
get_hugefile_by_virt_addr(uint64_t virt_addr, struct memseg_cache_entry * e)
{
	uint64_t start_addr, end_addr;
	char *start,*path_end;
	char buf[PATH_MAX*2];
	FILE *f;

	start = NULL;
	path_end = NULL;
	start_addr = 0;

	memset(e->filepath, 0, sizeof(e->filepath));

	/* open /proc/self/maps */
	f = fopen("/proc/self/maps", "r");
	if (f == NULL) {
		RTE_LOG(ERR, EAL, "cannot open /proc/self/maps!\n");
		return -1;
	}

	/* parse maps */
	while (fgets(buf, sizeof(buf), f) != NULL) {

		/* get endptr to end of start addr */
		start = buf;

		GET_PAGEMAP_ADDR(start,start_addr,'-',
				"Cannot find start address in maps!\n");

		/* if start address is bigger than our address, skip */
		if (start_addr > virt_addr)
			continue;

		GET_PAGEMAP_ADDR(start,end_addr,' ',
				"Cannot find end address in maps!\n");

		/* if end address is less than our address, skip */
		if (end_addr <= virt_addr)
			continue;

		/* find where the path starts */
		start = strstr(start, "/");

		if (start == NULL)
			continue;

		/* at this point, we know that this is our map.
		 * now let's find the file */
		path_end = strstr(start, "\n");
		break;
	}

	if (path_end == NULL) {
		RTE_LOG(ERR, EAL, "Hugefile path not found!\n");
		goto error;
	}

	/* calculate offset and copy the file path */
	snprintf(e->filepath, RTE_PTR_DIFF(path_end, start) + 1, "%s", start);

	e->offset = virt_addr - start_addr;

	fclose(f);

	return 0;
error:
	fclose(f);
	return -1;
}

/*
 * This is a complex function. What it does is the following:
 *  1. Goes through metadata and gets list of hugepages involved
 *  2. Sorts the hugepages by size (1G first)
 *  3. Goes through metadata again and writes correct offsets
 *  4. Goes through pages and finds out their filenames, offsets etc.
 */
static int
build_config(struct rte_ivshmem_metadata * metadata)
{
	struct rte_ivshmem_metadata_entry * e_local;
	struct memseg_cache_entry * ms_local;
	struct rte_memseg pages[IVSHMEM_MAX_PAGES];
	struct rte_ivshmem_metadata_entry *entry;
	struct memseg_cache_entry * c_entry, * prev_entry;
	struct ivshmem_config * config;
	unsigned i, j, mz_iter, ms_iter;
	uint64_t biggest_len;
	int biggest_idx;

	/* return error if we try to use an unknown config file */
	config = get_config_by_name(metadata->name);
	if (config == NULL) {
		RTE_LOG(ERR, EAL, "Cannot find IVSHMEM config %s!\n", metadata->name);
		goto fail_e;
	}

	memset(pages, 0, sizeof(pages));

	e_local = malloc(sizeof(config->metadata->entry));
	if (e_local == NULL)
		goto fail_e;
	ms_local = malloc(sizeof(config->memseg_cache));
	if (ms_local == NULL)
		goto fail_ms;


	/* make local copies before doing anything */
	memcpy(e_local, config->metadata->entry, sizeof(config->metadata->entry));
	memcpy(ms_local, config->memseg_cache, sizeof(config->memseg_cache));

	qsort(e_local, RTE_DIM(config->metadata->entry), sizeof(struct rte_ivshmem_metadata_entry),
			entry_compare);

	/* first pass - collect all huge pages */
	for (mz_iter = 0; mz_iter < RTE_DIM(config->metadata->entry); mz_iter++) {

		entry = &e_local[mz_iter];

		uint64_t start_addr = RTE_ALIGN_FLOOR(entry->mz.addr_64,
				entry->mz.hugepage_sz);
		uint64_t offset = entry->mz.addr_64 - start_addr;
		uint64_t len = RTE_ALIGN_CEIL(entry->mz.len + offset,
				entry->mz.hugepage_sz);

		if (entry->mz.addr_64 == 0 || start_addr == 0 || len == 0)
			continue;

		int start_page;

		/* find first unused page - mz are phys_addr sorted so we don't have to
		 * look out for holes */
		for (i = 0; i < RTE_DIM(pages); i++) {

			/* skip if we already have this page */
			if (pages[i].addr_64 == start_addr) {
				start_addr += entry->mz.hugepage_sz;
				len -= entry->mz.hugepage_sz;
				continue;
			}
			/* we found a new page */
			else if (pages[i].addr_64 == 0) {
				start_page = i;
				break;
			}
		}
		if (i == RTE_DIM(pages)) {
			RTE_LOG(ERR, EAL, "Cannot find unused page!\n");
			goto fail;
		}

		/* populate however many pages the memzone has */
		for (i = start_page; i < RTE_DIM(pages) && len != 0; i++) {

			pages[i].addr_64 = start_addr;
			pages[i].len = entry->mz.hugepage_sz;
			start_addr += entry->mz.hugepage_sz;
			len -= entry->mz.hugepage_sz;
		}
		/* if there's still length left */
		if (len != 0) {
			RTE_LOG(ERR, EAL, "Not enough space for pages!\n");
			goto fail;
		}
	}

	/* second pass - sort pages by size */
	for (i = 0; i < RTE_DIM(pages); i++) {

		if (pages[i].addr == NULL)
			break;

		biggest_len = 0;
		biggest_idx = -1;

		/*
		 * browse all entries starting at 'i', and find the
		 * entry with the smallest addr
		 */
		for (j=i; j< RTE_DIM(pages); j++) {
			if (pages[j].addr == NULL)
					break;
			if (biggest_len == 0 ||
				pages[j].len > biggest_len) {
				biggest_len = pages[j].len;
				biggest_idx = j;
			}
		}

		/* should not happen */
		if (biggest_idx == -1) {
			RTE_LOG(ERR, EAL, "Error sorting by size!\n");
			goto fail;
		}
		if (i != (unsigned) biggest_idx) {
			struct rte_memseg tmp;

			memcpy(&tmp, &pages[biggest_idx], sizeof(struct rte_memseg));

			/* we don't want to break contiguousness, so instead of just
			 * swapping segments, we move all the preceding segments to the
			 * right and then put the old segment @ biggest_idx in place of
			 * segment @ i */
			for (j = biggest_idx - 1; j >= i; j--) {
				memcpy(&pages[j+1], &pages[j], sizeof(struct rte_memseg));
				memset(&pages[j], 0, sizeof(struct rte_memseg));
			}

			/* put old biggest segment to its new place */
			memcpy(&pages[i], &tmp, sizeof(struct rte_memseg));
		}
	}

	/* third pass - write correct offsets */
	for (mz_iter = 0; mz_iter < RTE_DIM(config->metadata->entry); mz_iter++) {

		uint64_t offset = 0;

		entry = &e_local[mz_iter];

		if (entry->mz.addr_64 == 0)
			break;

		/* find page for current memzone */
		for (i = 0; i < RTE_DIM(pages); i++) {
			/* we found our page */
			if (entry->mz.addr_64 >= pages[i].addr_64 &&
					entry->mz.addr_64 < pages[i].addr_64 + pages[i].len) {
				entry->offset = (entry->mz.addr_64 - pages[i].addr_64) +
						offset;
				break;
			}
			offset += pages[i].len;
		}
		if (i == RTE_DIM(pages)) {
			RTE_LOG(ERR, EAL, "Page not found!\n");
			goto fail;
		}
	}

	ms_iter = 0;
	prev_entry = NULL;

	/* fourth pass - create proper memseg cache */
	for (i = 0; i < RTE_DIM(pages) &&
			ms_iter <= RTE_DIM(config->memseg_cache); i++) {
		if (pages[i].addr_64 == 0)
			break;


		if (ms_iter == RTE_DIM(pages)) {
			RTE_LOG(ERR, EAL, "The universe has collapsed!\n");
			goto fail;
		}

		c_entry = &ms_local[ms_iter];
		c_entry->len = pages[i].len;

		if (get_hugefile_by_virt_addr(pages[i].addr_64, c_entry) < 0)
			goto fail;

		/* if previous entry has the same filename and is contiguous,
		 * clear current entry and increase previous entry's length
		 */
		if (prev_entry != NULL &&
				strncmp(c_entry->filepath, prev_entry->filepath,
				sizeof(c_entry->filepath)) == 0 &&
				prev_entry->offset + prev_entry->len == c_entry->offset) {
			prev_entry->len += pages[i].len;
			memset(c_entry, 0, sizeof(struct memseg_cache_entry));
		}
		else {
			prev_entry = c_entry;
			ms_iter++;
		}
	}

	/* update current configuration with new valid data */
	memcpy(config->metadata->entry, e_local, sizeof(config->metadata->entry));
	memcpy(config->memseg_cache, ms_local, sizeof(config->memseg_cache));

	free(ms_local);
	free(e_local);

	return 0;
fail:
	free(ms_local);
fail_ms:
	free(e_local);
fail_e:
	return -1;
}

static int
add_memzone_to_metadata(const struct rte_memzone * mz,
		struct ivshmem_config * config)
{
	struct rte_ivshmem_metadata_entry * entry;
	unsigned i;

	rte_spinlock_lock(&config->sl);

	/* find free slot in this config */
	for (i = 0; i < RTE_DIM(config->metadata->entry); i++) {
		entry = &config->metadata->entry[i];

		if (&entry->mz.addr_64 != 0 && overlap(mz, &entry->mz)) {
			RTE_LOG(ERR, EAL, "Overlapping memzones!\n");
			goto fail;
		}

		/* if addr is zero, the memzone is probably free */
		if (entry->mz.addr_64 == 0) {
			RTE_LOG(DEBUG, EAL, "Adding memzone '%s' at %p to metadata %s\n",
					mz->name, mz->addr, config->metadata->name);
			memcpy(&entry->mz, mz, sizeof(struct rte_memzone));

			/* run config file parser */
			if (build_config(config->metadata) < 0)
				goto fail;

			break;
		}
	}

	/* if we reached the maximum, that means we have no place in config */
	if (i == RTE_DIM(config->metadata->entry)) {
		RTE_LOG(ERR, EAL, "No space left in IVSHMEM metadata %s!\n",
				config->metadata->name);
		goto fail;
	}

	rte_spinlock_unlock(&config->sl);
	return 0;
fail:
	rte_spinlock_unlock(&config->sl);
	return -1;
}

static int
add_ring_to_metadata(const struct rte_ring * r,
		struct ivshmem_config * config)
{
	struct rte_memzone * mz;

	mz = get_memzone_by_addr(r);

	if (!mz) {
		RTE_LOG(ERR, EAL, "Cannot find memzone for ring!\n");
		return -1;
	}

	return add_memzone_to_metadata(mz, config);
}

static int
add_mempool_to_metadata(const struct rte_mempool * mp,
		struct ivshmem_config * config)
{
	struct rte_memzone * mz;
	int ret;

	mz = get_memzone_by_addr(mp);
	ret = 0;

	if (!mz) {
		RTE_LOG(ERR, EAL, "Cannot find memzone for mempool!\n");
		return -1;
	}

	/* mempool consists of memzone and ring */
	ret = add_memzone_to_metadata(mz, config);
	if (ret < 0)
		return -1;

	return add_ring_to_metadata(mp->ring, config);
}

int
rte_ivshmem_metadata_add_ring(const struct rte_ring * r, const char * name)
{
	struct ivshmem_config * config;

	if (name == NULL || r == NULL)
		return -1;

	config = get_config_by_name(name);

	if (config == NULL) {
		RTE_LOG(ERR, EAL, "Cannot find IVSHMEM config %s!\n", name);
		return -1;
	}

	return add_ring_to_metadata(r, config);
}

int
rte_ivshmem_metadata_add_memzone(const struct rte_memzone * mz, const char * name)
{
	struct ivshmem_config * config;

	if (name == NULL || mz == NULL)
		return -1;

	config = get_config_by_name(name);

	if (config == NULL) {
		RTE_LOG(ERR, EAL, "Cannot find IVSHMEM config %s!\n", name);
		return -1;
	}

	return add_memzone_to_metadata(mz, config);
}

int
rte_ivshmem_metadata_add_mempool(const struct rte_mempool * mp, const char * name)
{
	struct ivshmem_config * config;

	if (name == NULL || mp == NULL)
		return -1;

	config = get_config_by_name(name);

	if (config == NULL) {
		RTE_LOG(ERR, EAL, "Cannot find IVSHMEM config %s!\n", name);
		return -1;
	}

	return add_mempool_to_metadata(mp, config);
}

static inline void
ivshmem_config_path(char *buffer, size_t bufflen, const char *name)
{
	snprintf(buffer, bufflen, IVSHMEM_CONFIG_FILE_FMT, name);
}



static inline
void *ivshmem_metadata_create(const char *name, size_t size,
		struct flock *lock)
{
	int retval, fd;
	void *metadata_addr;
	char pathname[PATH_MAX];

	ivshmem_config_path(pathname, sizeof(pathname), name);

	fd = open(pathname, O_RDWR | O_CREAT, 0660);
	if (fd < 0) {
		RTE_LOG(ERR, EAL, "Cannot open '%s'\n", pathname);
		return NULL;
	}

	size = METADATA_SIZE_ALIGNED;

	retval = fcntl(fd, F_SETLK, lock);
	if (retval < 0){
		close(fd);
		RTE_LOG(ERR, EAL, "Cannot create lock on '%s'. Is another "
				"process using it?\n", pathname);
		return NULL;
	}

	retval = ftruncate(fd, size);
	if (retval < 0){
		close(fd);
		RTE_LOG(ERR, EAL, "Cannot resize '%s'\n", pathname);
		return NULL;
	}

	metadata_addr = mmap(NULL, size,
				PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (metadata_addr == MAP_FAILED){
		RTE_LOG(ERR, EAL, "Cannot mmap memory for '%s'\n", pathname);

		/* we don't care if we can't unlock */
		fcntl(fd, F_UNLCK, lock);
		close(fd);

		return NULL;
	}

	return metadata_addr;
}

int rte_ivshmem_metadata_create(const char *name)
{
	struct ivshmem_config * ivshmem_config;
	unsigned index;

	if (pagesz == 0)
		pagesz = getpagesize();

	if (name == NULL)
		return -1;

	rte_spinlock_lock(&global_cfg_sl);

	for (index = 0; index < RTE_DIM(ivshmem_global_config); index++) {
		if (ivshmem_global_config[index].metadata == NULL) {
			ivshmem_config = &ivshmem_global_config[index];
			break;
		}
	}

	if (index == RTE_DIM(ivshmem_global_config)) {
		RTE_LOG(ERR, EAL, "Cannot create more ivshmem config files. "
		"Maximum has been reached\n");
		rte_spinlock_unlock(&global_cfg_sl);
		return -1;
	}

	ivshmem_config->lock.l_type = F_WRLCK;
	ivshmem_config->lock.l_whence = SEEK_SET;

	ivshmem_config->lock.l_start = 0;
	ivshmem_config->lock.l_len = METADATA_SIZE_ALIGNED;

	ivshmem_global_config[index].metadata = ((struct rte_ivshmem_metadata *)
			ivshmem_metadata_create(
					name,
					sizeof(struct rte_ivshmem_metadata),
					&ivshmem_config->lock));

	if (ivshmem_global_config[index].metadata == NULL) {
		rte_spinlock_unlock(&global_cfg_sl);
		return -1;
	}

	/* Metadata setup */
	memset(ivshmem_config->metadata, 0, sizeof(struct rte_ivshmem_metadata));
	ivshmem_config->metadata->magic_number = IVSHMEM_MAGIC;
	snprintf(ivshmem_config->metadata->name,
			sizeof(ivshmem_config->metadata->name), "%s", name);

	rte_spinlock_unlock(&global_cfg_sl);

	return 0;
}

int
rte_ivshmem_metadata_cmdline_generate(char *buffer, unsigned size, const char *name)
{
	const struct memseg_cache_entry * ms_cache, *entry;
	struct ivshmem_config * config;
	char cmdline[IVSHMEM_QEMU_CMDLINE_BUFSIZE], *cmdline_ptr;
	char cfg_file_path[PATH_MAX];
	unsigned remaining_len, tmplen, iter;
	uint64_t shared_mem_size, zero_size, total_size;

	if (buffer == NULL || name == NULL)
		return -1;

	config = get_config_by_name(name);

	if (config == NULL) {
		RTE_LOG(ERR, EAL, "Config %s not found!\n", name);
		return -1;
	}

	rte_spinlock_lock(&config->sl);

	/* prepare metadata file path */
	snprintf(cfg_file_path, sizeof(cfg_file_path), IVSHMEM_CONFIG_FILE_FMT,
			config->metadata->name);

	ms_cache = config->memseg_cache;

	cmdline_ptr = cmdline;
	remaining_len = sizeof(cmdline);

	shared_mem_size = 0;
	iter = 0;

	while ((ms_cache[iter].len != 0) && (iter < RTE_DIM(config->metadata->entry))) {

		entry = &ms_cache[iter];

		/* Offset and sizes within the current pathname */
		tmplen = snprintf(cmdline_ptr, remaining_len, IVSHMEM_QEMU_CMD_FD_FMT,
				entry->filepath, entry->offset, entry->len);

		shared_mem_size += entry->len;

		cmdline_ptr = RTE_PTR_ADD(cmdline_ptr, tmplen);
		remaining_len -= tmplen;

		if (remaining_len == 0) {
			RTE_LOG(ERR, EAL, "Command line too long!\n");
			rte_spinlock_unlock(&config->sl);
			return -1;
		}

		iter++;
	}

	total_size = rte_align64pow2(shared_mem_size + METADATA_SIZE_ALIGNED);
	zero_size = total_size - shared_mem_size - METADATA_SIZE_ALIGNED;

	/* add /dev/zero to command-line to fill the space */
	tmplen = snprintf(cmdline_ptr, remaining_len, IVSHMEM_QEMU_CMD_FD_FMT,
			"/dev/zero",
			(uint64_t)0x0,
			zero_size);

	cmdline_ptr = RTE_PTR_ADD(cmdline_ptr, tmplen);
	remaining_len -= tmplen;

	if (remaining_len == 0) {
		RTE_LOG(ERR, EAL, "Command line too long!\n");
		rte_spinlock_unlock(&config->sl);
		return -1;
	}

	/* add metadata file to the end of command-line */
	tmplen = snprintf(cmdline_ptr, remaining_len, IVSHMEM_QEMU_CMD_FD_FMT,
			cfg_file_path,
			(uint64_t)0x0,
			METADATA_SIZE_ALIGNED);

	cmdline_ptr = RTE_PTR_ADD(cmdline_ptr, tmplen);
	remaining_len -= tmplen;

	if (remaining_len == 0) {
		RTE_LOG(ERR, EAL, "Command line too long!\n");
		rte_spinlock_unlock(&config->sl);
		return -1;
	}

	/* if current length of the command line is bigger than the buffer supplied
	 * by the user, or if command-line is bigger than what IVSHMEM accepts */
	if ((sizeof(cmdline) - remaining_len) > size) {
		RTE_LOG(ERR, EAL, "Buffer is too short!\n");
		rte_spinlock_unlock(&config->sl);
		return -1;
	}
	/* complete the command-line */
	snprintf(buffer, size,
			IVSHMEM_QEMU_CMD_LINE_HEADER_FMT,
			total_size >> 20,
			cmdline);

	rte_spinlock_unlock(&config->sl);

	return 0;
}

void
rte_ivshmem_metadata_dump(FILE *f, const char *name)
{
	unsigned i = 0;
	struct ivshmem_config * config;
	struct rte_ivshmem_metadata_entry *entry;
#ifdef RTE_LIBRTE_IVSHMEM_DEBUG
	uint64_t addr;
	uint64_t end, hugepage_sz;
	struct memseg_cache_entry e;
#endif

	if (name == NULL)
		return;

	/* return error if we try to use an unknown config file */
	config = get_config_by_name(name);
	if (config == NULL) {
		RTE_LOG(ERR, EAL, "Cannot find IVSHMEM config %s!\n", name);
		return;
	}

	rte_spinlock_lock(&config->sl);

	entry = &config->metadata->entry[0];

	while (entry->mz.addr != NULL && i < RTE_DIM(config->metadata->entry)) {

		fprintf(f, "Entry %u: name:<%-20s>, phys:0x%-15lx, len:0x%-15lx, "
			"virt:%-15p, off:0x%-15lx\n",
			i,
			entry->mz.name,
			entry->mz.phys_addr,
			entry->mz.len,
			entry->mz.addr,
			entry->offset);
		i++;

#ifdef RTE_LIBRTE_IVSHMEM_DEBUG
		fprintf(f, "\tHugepage files:\n");

		hugepage_sz = entry->mz.hugepage_sz;
		addr = RTE_ALIGN_FLOOR(entry->mz.addr_64, hugepage_sz);
		end = addr + RTE_ALIGN_CEIL(entry->mz.len + (entry->mz.addr_64 - addr),
				hugepage_sz);

		for (; addr < end; addr += hugepage_sz) {
			memset(&e, 0, sizeof(e));

			get_hugefile_by_virt_addr(addr, &e);

			fprintf(f, "\t0x%"PRIx64 "-0x%" PRIx64 " offset: 0x%" PRIx64 " %s\n",
					addr, addr + hugepage_sz, e.offset, e.filepath);
		}
#endif
		entry++;
	}

	rte_spinlock_unlock(&config->sl);
}
