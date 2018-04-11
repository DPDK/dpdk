/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#define _FILE_OFFSET_BITS 64
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <sys/file.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <signal.h>
#include <setjmp.h>
#ifdef RTE_EAL_NUMA_AWARE_HUGEPAGES
#include <numa.h>
#include <numaif.h>
#endif

#include <rte_common.h>
#include <rte_log.h>
#include <rte_eal_memconfig.h>
#include <rte_eal.h>
#include <rte_memory.h>
#include <rte_spinlock.h>

#include "eal_filesystem.h"
#include "eal_internal_cfg.h"
#include "eal_memalloc.h"

static sigjmp_buf huge_jmpenv;

static void __rte_unused huge_sigbus_handler(int signo __rte_unused)
{
	siglongjmp(huge_jmpenv, 1);
}

/* Put setjmp into a wrap method to avoid compiling error. Any non-volatile,
 * non-static local variable in the stack frame calling sigsetjmp might be
 * clobbered by a call to longjmp.
 */
static int __rte_unused huge_wrap_sigsetjmp(void)
{
	return sigsetjmp(huge_jmpenv, 1);
}

static struct sigaction huge_action_old;
static int huge_need_recover;

static void __rte_unused
huge_register_sigbus(void)
{
	sigset_t mask;
	struct sigaction action;

	sigemptyset(&mask);
	sigaddset(&mask, SIGBUS);
	action.sa_flags = 0;
	action.sa_mask = mask;
	action.sa_handler = huge_sigbus_handler;

	huge_need_recover = !sigaction(SIGBUS, &action, &huge_action_old);
}

static void __rte_unused
huge_recover_sigbus(void)
{
	if (huge_need_recover) {
		sigaction(SIGBUS, &huge_action_old, NULL);
		huge_need_recover = 0;
	}
}

#ifdef RTE_EAL_NUMA_AWARE_HUGEPAGES
static bool
check_numa(void)
{
	bool ret = true;
	/* Check if kernel supports NUMA. */
	if (numa_available() != 0) {
		RTE_LOG(DEBUG, EAL, "NUMA is not supported.\n");
		ret = false;
	}
	return ret;
}

static void
prepare_numa(int *oldpolicy, struct bitmask *oldmask, int socket_id)
{
	RTE_LOG(DEBUG, EAL, "Trying to obtain current memory policy.\n");
	if (get_mempolicy(oldpolicy, oldmask->maskp,
			  oldmask->size + 1, 0, 0) < 0) {
		RTE_LOG(ERR, EAL,
			"Failed to get current mempolicy: %s. "
			"Assuming MPOL_DEFAULT.\n", strerror(errno));
		oldpolicy = MPOL_DEFAULT;
	}
	RTE_LOG(DEBUG, EAL,
		"Setting policy MPOL_PREFERRED for socket %d\n",
		socket_id);
	numa_set_preferred(socket_id);
}

static void
resotre_numa(int *oldpolicy, struct bitmask *oldmask)
{
	RTE_LOG(DEBUG, EAL,
		"Restoring previous memory policy: %d\n", *oldpolicy);
	if (oldpolicy == MPOL_DEFAULT) {
		numa_set_localalloc();
	} else if (set_mempolicy(*oldpolicy, oldmask->maskp,
				 oldmask->size + 1) < 0) {
		RTE_LOG(ERR, EAL, "Failed to restore mempolicy: %s\n",
			strerror(errno));
		numa_set_localalloc();
	}
	numa_free_cpumask(oldmask);
}
#endif

static int
get_seg_fd(char *path, int buflen, struct hugepage_info *hi,
		unsigned int list_idx, unsigned int seg_idx)
{
	int fd;
	eal_get_hugefile_path(path, buflen, hi->hugedir,
			list_idx * RTE_MAX_MEMSEG_PER_LIST + seg_idx);
	fd = open(path, O_CREAT | O_RDWR, 0600);
	if (fd < 0) {
		RTE_LOG(DEBUG, EAL, "%s(): open failed: %s\n", __func__,
				strerror(errno));
		return -1;
	}
	return fd;
}

/* returns 1 on successful lock, 0 on unsuccessful lock, -1 on error */
static int lock(int fd, uint64_t offset, uint64_t len, int type)
{
	struct flock lck;
	int ret;

	memset(&lck, 0, sizeof(lck));

	lck.l_type = type;
	lck.l_whence = SEEK_SET;
	lck.l_start = offset;
	lck.l_len = len;

	ret = fcntl(fd, F_SETLK, &lck);

	if (ret && (errno == EAGAIN || errno == EACCES)) {
		/* locked by another process, not an error */
		return 0;
	} else if (ret) {
		RTE_LOG(ERR, EAL, "%s(): error calling fcntl(): %s\n",
			__func__, strerror(errno));
		/* we've encountered an unexpected error */
		return -1;
	}
	return 1;
}

static int
alloc_seg(struct rte_memseg *ms, void *addr, int socket_id,
		struct hugepage_info *hi, unsigned int list_idx,
		unsigned int seg_idx)
{
#ifdef RTE_EAL_NUMA_AWARE_HUGEPAGES
	int cur_socket_id = 0;
#endif
	uint64_t map_offset;
	char path[PATH_MAX];
	int ret = 0;
	int fd;
	size_t alloc_sz;

	fd = get_seg_fd(path, sizeof(path), hi, list_idx, seg_idx);
	if (fd < 0)
		return -1;

	alloc_sz = hi->hugepage_sz;

	map_offset = 0;
	if (ftruncate(fd, alloc_sz) < 0) {
		RTE_LOG(DEBUG, EAL, "%s(): ftruncate() failed: %s\n",
			__func__, strerror(errno));
		goto resized;
	}
	/* we've allocated a page - take out a read lock. we're using fcntl()
	 * locks rather than flock() here because doing that gives us one huge
	 * advantage - fcntl() locks are per-process, not per-file descriptor,
	 * which means that we don't have to keep the original fd's around to
	 * keep a lock on the file.
	 *
	 * this is useful, because when it comes to unmapping pages, we will
	 * have to take out a write lock (to figure out if another process still
	 * has this page mapped), and to do itwith flock() we'll have to use
	 * original fd, as lock is associated with that particular fd. with
	 * fcntl(), this is not necessary - we can open a new fd and use fcntl()
	 * on that.
	 */
	ret = lock(fd, map_offset, alloc_sz, F_RDLCK);

	/* this should not fail */
	if (ret != 1) {
		RTE_LOG(ERR, EAL, "%s(): error locking file: %s\n",
			__func__,
			strerror(errno));
		goto resized;
	}

	/*
	 * map the segment, and populate page tables, the kernel fills this
	 * segment with zeros if it's a new page.
	 */
	void *va = mmap(addr, alloc_sz, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_POPULATE | MAP_FIXED, fd, map_offset);
	close(fd);

	if (va == MAP_FAILED) {
		RTE_LOG(DEBUG, EAL, "%s(): mmap() failed: %s\n", __func__,
			strerror(errno));
		goto resized;
	}
	if (va != addr) {
		RTE_LOG(DEBUG, EAL, "%s(): wrong mmap() address\n", __func__);
		goto mapped;
	}

	rte_iova_t iova = rte_mem_virt2iova(addr);
	if (iova == RTE_BAD_PHYS_ADDR) {
		RTE_LOG(DEBUG, EAL, "%s(): can't get IOVA addr\n",
			__func__);
		goto mapped;
	}

#ifdef RTE_EAL_NUMA_AWARE_HUGEPAGES
	move_pages(getpid(), 1, &addr, NULL, &cur_socket_id, 0);

	if (cur_socket_id != socket_id) {
		RTE_LOG(DEBUG, EAL,
				"%s(): allocation happened on wrong socket (wanted %d, got %d)\n",
			__func__, socket_id, cur_socket_id);
		goto mapped;
	}
#endif

	/* In linux, hugetlb limitations, like cgroup, are
	 * enforced at fault time instead of mmap(), even
	 * with the option of MAP_POPULATE. Kernel will send
	 * a SIGBUS signal. To avoid to be killed, save stack
	 * environment here, if SIGBUS happens, we can jump
	 * back here.
	 */
	if (huge_wrap_sigsetjmp()) {
		RTE_LOG(DEBUG, EAL, "SIGBUS: Cannot mmap more hugepages of size %uMB\n",
			(unsigned int)(alloc_sz >> 20));
		goto mapped;
	}
	*(int *)addr = *(int *)addr;

	ms->addr = addr;
	ms->hugepage_sz = alloc_sz;
	ms->len = alloc_sz;
	ms->nchannel = rte_memory_get_nchannel();
	ms->nrank = rte_memory_get_nrank();
	ms->iova = iova;
	ms->socket_id = socket_id;

	return 0;

mapped:
	munmap(addr, alloc_sz);
resized:
	close(fd);
	unlink(path);
	return -1;
}

struct alloc_walk_param {
	struct hugepage_info *hi;
	struct rte_memseg **ms;
	size_t page_sz;
	unsigned int segs_allocated;
	unsigned int n_segs;
	int socket;
	bool exact;
};
static int
alloc_seg_walk(const struct rte_memseg_list *msl, void *arg)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct alloc_walk_param *wa = arg;
	struct rte_memseg_list *cur_msl;
	size_t page_sz;
	int cur_idx;
	unsigned int msl_idx, need, i;

	if (msl->page_sz != wa->page_sz)
		return 0;
	if (msl->socket_id != wa->socket)
		return 0;

	page_sz = (size_t)msl->page_sz;

	msl_idx = msl - mcfg->memsegs;
	cur_msl = &mcfg->memsegs[msl_idx];

	need = wa->n_segs;

	/* try finding space in memseg list */
	cur_idx = rte_fbarray_find_next_n_free(&cur_msl->memseg_arr, 0, need);
	if (cur_idx < 0)
		return 0;

	for (i = 0; i < need; i++, cur_idx++) {
		struct rte_memseg *cur;
		void *map_addr;

		cur = rte_fbarray_get(&cur_msl->memseg_arr, cur_idx);
		map_addr = RTE_PTR_ADD(cur_msl->base_va,
				cur_idx * page_sz);

		if (alloc_seg(cur, map_addr, wa->socket, wa->hi,
				msl_idx, cur_idx)) {
			RTE_LOG(DEBUG, EAL, "attempted to allocate %i segments, but only %i were allocated\n",
				need, i);

			/* if exact number wasn't requested, stop */
			if (!wa->exact)
				goto out;
			return -1;
		}
		if (wa->ms)
			wa->ms[i] = cur;

		rte_fbarray_set_used(&cur_msl->memseg_arr, cur_idx);
	}
out:
	wa->segs_allocated = i;
	return 1;

}

int
eal_memalloc_alloc_seg_bulk(struct rte_memseg **ms, int n_segs, size_t page_sz,
		int socket, bool exact)
{
	int i, ret = -1;
#ifdef RTE_EAL_NUMA_AWARE_HUGEPAGES
	bool have_numa = false;
	int oldpolicy;
	struct bitmask *oldmask;
#endif
	struct alloc_walk_param wa;
	struct hugepage_info *hi = NULL;

	memset(&wa, 0, sizeof(wa));

	/* dynamic allocation not supported in legacy mode */
	if (internal_config.legacy_mem)
		return -1;

	for (i = 0; i < (int) RTE_DIM(internal_config.hugepage_info); i++) {
		if (page_sz ==
				internal_config.hugepage_info[i].hugepage_sz) {
			hi = &internal_config.hugepage_info[i];
			break;
		}
	}
	if (!hi) {
		RTE_LOG(ERR, EAL, "%s(): can't find relevant hugepage_info entry\n",
			__func__);
		return -1;
	}

#ifdef RTE_EAL_NUMA_AWARE_HUGEPAGES
	if (check_numa()) {
		oldmask = numa_allocate_nodemask();
		prepare_numa(&oldpolicy, oldmask, socket);
		have_numa = true;
	}
#endif

	wa.exact = exact;
	wa.hi = hi;
	wa.ms = ms;
	wa.n_segs = n_segs;
	wa.page_sz = page_sz;
	wa.socket = socket;
	wa.segs_allocated = 0;

	ret = rte_memseg_list_walk(alloc_seg_walk, &wa);
	if (ret == 0) {
		RTE_LOG(ERR, EAL, "%s(): couldn't find suitable memseg_list\n",
			__func__);
		ret = -1;
	} else if (ret > 0) {
		ret = (int)wa.segs_allocated;
	}

#ifdef RTE_EAL_NUMA_AWARE_HUGEPAGES
	if (have_numa)
		resotre_numa(&oldpolicy, oldmask);
#endif
	return ret;
}

struct rte_memseg *
eal_memalloc_alloc_seg(size_t page_sz, int socket)
{
	struct rte_memseg *ms;
	if (eal_memalloc_alloc_seg_bulk(&ms, 1, page_sz, socket, true) < 0)
		return NULL;
	/* return pointer to newly allocated memseg */
	return ms;
}
