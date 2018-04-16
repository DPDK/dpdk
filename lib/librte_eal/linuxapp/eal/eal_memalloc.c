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
#include <linux/falloc.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_eal_memconfig.h>
#include <rte_eal.h>
#include <rte_memory.h>
#include <rte_spinlock.h>

#include "eal_filesystem.h"
#include "eal_internal_cfg.h"
#include "eal_memalloc.h"

/*
 * not all kernel version support fallocate on hugetlbfs, so fall back to
 * ftruncate and disallow deallocation if fallocate is not supported.
 */
static int fallocate_supported = -1; /* unknown */

/*
 * If each page is in a separate file, we can close fd's since we need each fd
 * only once. However, in single file segments mode, we can get away with using
 * a single fd for entire segments, but we need to store them somewhere. Each
 * fd is different within each process, so we'll store them in a local tailq.
 */
struct msl_entry {
	TAILQ_ENTRY(msl_entry) next;
	unsigned int msl_idx;
	int fd;
};

/** Double linked list of memseg list fd's. */
TAILQ_HEAD(msl_entry_list, msl_entry);

static struct msl_entry_list msl_entry_list =
		TAILQ_HEAD_INITIALIZER(msl_entry_list);
static rte_spinlock_t tailq_lock = RTE_SPINLOCK_INITIALIZER;

/** local copy of a memory map, used to synchronize memory hotplug in MP */
static struct rte_memseg_list local_memsegs[RTE_MAX_MEMSEG_LISTS];

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
	if (*oldpolicy == MPOL_DEFAULT) {
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

static struct msl_entry *
get_msl_entry_by_idx(unsigned int list_idx)
{
	struct msl_entry *te;

	rte_spinlock_lock(&tailq_lock);

	TAILQ_FOREACH(te, &msl_entry_list, next) {
		if (te->msl_idx == list_idx)
			break;
	}
	if (te == NULL) {
		/* doesn't exist, so create it and set fd to -1 */

		te = malloc(sizeof(*te));
		if (te == NULL) {
			RTE_LOG(ERR, EAL, "%s(): cannot allocate tailq entry for memseg list\n",
				__func__);
			goto unlock;
		}
		te->msl_idx = list_idx;
		te->fd = -1;
		TAILQ_INSERT_TAIL(&msl_entry_list, te, next);
	}
unlock:
	rte_spinlock_unlock(&tailq_lock);
	return te;
}

/*
 * uses fstat to report the size of a file on disk
 */
static off_t
get_file_size(int fd)
{
	struct stat st;
	if (fstat(fd, &st) < 0)
		return 0;
	return st.st_size;
}

/*
 * uses fstat to check if file size on disk is zero (regular fstat won't show
 * true file size due to how fallocate works)
 */
static bool
is_zero_length(int fd)
{
	struct stat st;
	if (fstat(fd, &st) < 0)
		return false;
	return st.st_blocks == 0;
}

/* we cannot use rte_memseg_list_walk() here because we will be holding a
 * write lock whenever we enter every function in this file, however copying
 * the same iteration code everywhere is not ideal as well. so, use a lockless
 * copy of memseg list walk here.
 */
static int
memseg_list_walk_thread_unsafe(rte_memseg_list_walk_t func, void *arg)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	int i, ret = 0;

	for (i = 0; i < RTE_MAX_MEMSEG_LISTS; i++) {
		struct rte_memseg_list *msl = &mcfg->memsegs[i];

		if (msl->base_va == NULL)
			continue;

		ret = func(msl, arg);
		if (ret < 0)
			return -1;
		if (ret > 0)
			return 1;
	}
	return 0;
}

static int
get_seg_fd(char *path, int buflen, struct hugepage_info *hi,
		unsigned int list_idx, unsigned int seg_idx)
{
	int fd;

	if (internal_config.single_file_segments) {
		/*
		 * try to find a tailq entry, for this memseg list, or create
		 * one if it doesn't exist.
		 */
		struct msl_entry *te = get_msl_entry_by_idx(list_idx);
		if (te == NULL) {
			RTE_LOG(ERR, EAL, "%s(): cannot allocate tailq entry for memseg list\n",
				__func__);
			return -1;
		} else if (te->fd < 0) {
			/* create a hugepage file */
			eal_get_hugefile_path(path, buflen, hi->hugedir,
					list_idx);
			fd = open(path, O_CREAT | O_RDWR, 0600);
			if (fd < 0) {
				RTE_LOG(DEBUG, EAL, "%s(): open failed: %s\n",
					__func__, strerror(errno));
				return -1;
			}
			te->fd = fd;
		} else {
			fd = te->fd;
		}
	} else {
		/* one file per page, just create it */
		eal_get_hugefile_path(path, buflen, hi->hugedir,
				list_idx * RTE_MAX_MEMSEG_PER_LIST + seg_idx);
		fd = open(path, O_CREAT | O_RDWR, 0600);
		if (fd < 0) {
			RTE_LOG(DEBUG, EAL, "%s(): open failed: %s\n", __func__,
					strerror(errno));
			return -1;
		}
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
resize_hugefile(int fd, uint64_t fa_offset, uint64_t page_sz,
		bool grow)
{
	bool again = false;
	do {
		if (fallocate_supported == 0) {
			/* we cannot deallocate memory if fallocate() is not
			 * supported, but locks are still needed to prevent
			 * primary process' initialization from clearing out
			 * huge pages used by this process.
			 */

			if (!grow) {
				RTE_LOG(DEBUG, EAL, "%s(): fallocate not supported, not freeing page back to the system\n",
					__func__);
				return -1;
			}
			uint64_t new_size = fa_offset + page_sz;
			uint64_t cur_size = get_file_size(fd);

			/* fallocate isn't supported, fall back to ftruncate */
			if (new_size > cur_size &&
					ftruncate(fd, new_size) < 0) {
				RTE_LOG(DEBUG, EAL, "%s(): ftruncate() failed: %s\n",
					__func__, strerror(errno));
				return -1;
			}
			/* not being able to take out a read lock is an error */
			if (lock(fd, fa_offset, page_sz, F_RDLCK) != 1)
				return -1;
		} else {
			int flags = grow ? 0 : FALLOC_FL_PUNCH_HOLE |
					FALLOC_FL_KEEP_SIZE;
			int ret;

			/* if fallocate() is supported, we need to take out a
			 * read lock on allocate (to prevent other processes
			 * from deallocating this page), and take out a write
			 * lock on deallocate (to ensure nobody else is using
			 * this page).
			 *
			 * we can't use flock() for this, as we actually need to
			 * lock part of the file, not the entire file.
			 */

			if (!grow) {
				ret = lock(fd, fa_offset, page_sz, F_WRLCK);

				if (ret < 0)
					return -1;
				else if (ret == 0)
					/* failed to lock, not an error */
					return 0;
			}
			if (fallocate(fd, flags, fa_offset, page_sz) < 0) {
				if (fallocate_supported == -1 &&
						errno == ENOTSUP) {
					RTE_LOG(ERR, EAL, "%s(): fallocate() not supported, hugepage deallocation will be disabled\n",
						__func__);
					again = true;
					fallocate_supported = 0;
				} else {
					RTE_LOG(DEBUG, EAL, "%s(): fallocate() failed: %s\n",
						__func__,
						strerror(errno));
					return -1;
				}
			} else {
				fallocate_supported = 1;

				if (grow) {
					/* if can't read lock, it's an error */
					if (lock(fd, fa_offset, page_sz,
							F_RDLCK) != 1)
						return -1;
				} else {
					/* if can't unlock, it's an error */
					if (lock(fd, fa_offset, page_sz,
							F_UNLCK) != 1)
						return -1;
				}
			}
		}
	} while (again);
	return 0;
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
	if (internal_config.single_file_segments) {
		map_offset = seg_idx * alloc_sz;
		ret = resize_hugefile(fd, map_offset, alloc_sz, true);
		if (ret < 0)
			goto resized;
	} else {
		map_offset = 0;
		if (ftruncate(fd, alloc_sz) < 0) {
			RTE_LOG(DEBUG, EAL, "%s(): ftruncate() failed: %s\n",
				__func__, strerror(errno));
			goto resized;
		}
		/* we've allocated a page - take out a read lock. we're using
		 * fcntl() locks rather than flock() here because doing that
		 * gives us one huge advantage - fcntl() locks are per-process,
		 * not per-file descriptor, which means that we don't have to
		 * keep the original fd's around to keep a lock on the file.
		 *
		 * this is useful, because when it comes to unmapping pages, we
		 * will have to take out a write lock (to figure out if another
		 * process still has this page mapped), and to do itwith flock()
		 * we'll have to use original fd, as lock is associated with
		 * that particular fd. with fcntl(), this is not necessary - we
		 * can open a new fd and use fcntl() on that.
		 */
		ret = lock(fd, map_offset, alloc_sz, F_RDLCK);

		/* this should not fail */
		if (ret != 1) {
			RTE_LOG(ERR, EAL, "%s(): error locking file: %s\n",
				__func__,
				strerror(errno));
			goto resized;
		}
	}

	/*
	 * map the segment, and populate page tables, the kernel fills this
	 * segment with zeros if it's a new page.
	 */
	void *va = mmap(addr, alloc_sz, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_POPULATE | MAP_FIXED, fd, map_offset);

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
	/* for non-single file segments, we can close fd here */
	if (!internal_config.single_file_segments)
		close(fd);

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
	if (internal_config.single_file_segments) {
		resize_hugefile(fd, map_offset, alloc_sz, false);
		if (is_zero_length(fd)) {
			struct msl_entry *te = get_msl_entry_by_idx(list_idx);
			/* te->fd is equivalent to fd */
			if (te != NULL && te->fd >= 0)
				te->fd = -1;
			/* ignore errors, can't make it any worse */
			unlink(path);
			close(fd);
		}
		/* if we're not removing the file, fd stays in the tailq */
	} else {
		close(fd);
		unlink(path);
	}
	return -1;
}

static int
free_seg(struct rte_memseg *ms, struct hugepage_info *hi,
		unsigned int list_idx, unsigned int seg_idx)
{
	uint64_t map_offset;
	char path[PATH_MAX];
	int fd, ret;

	/* erase page data */
	memset(ms->addr, 0, ms->len);

	if (mmap(ms->addr, ms->len, PROT_READ,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) ==
				MAP_FAILED) {
		RTE_LOG(DEBUG, EAL, "couldn't unmap page\n");
		return -1;
	}

	fd = get_seg_fd(path, sizeof(path), hi, list_idx, seg_idx);
	if (fd < 0)
		return -1;

	if (internal_config.single_file_segments) {
		map_offset = seg_idx * ms->len;
		if (resize_hugefile(fd, map_offset, ms->len, false))
			return -1;
		/* if file is zero-length, we've already shrunk it, so it's
		 * safe to remove.
		 */
		if (is_zero_length(fd)) {
			struct msl_entry *te = get_msl_entry_by_idx(list_idx);
			/* te->fd is equivalent to fd */
			if (te != NULL && te->fd >= 0)
				te->fd = -1;
			unlink(path);
			close(fd);
		}
		/* if we're not removing the file, fd stays in the tailq */
		ret = 0;
	} else {
		/* if we're able to take out a write lock, we're the last one
		 * holding onto this page.
		 */

		ret = lock(fd, 0, ms->len, F_WRLCK);
		if (ret >= 0) {
			/* no one else is using this page */
			if (ret == 1)
				unlink(path);
			ret = lock(fd, 0, ms->len, F_UNLCK);
			if (ret != 1)
				RTE_LOG(ERR, EAL, "%s(): unable to unlock file %s\n",
					__func__, path);
		}
		close(fd);
	}

	memset(ms, 0, sizeof(*ms));

	return ret;
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
	int cur_idx, start_idx, j;
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
	start_idx = cur_idx;

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

			/* clean up */
			for (j = start_idx; j < cur_idx; j++) {
				struct rte_memseg *tmp;
				struct rte_fbarray *arr =
						&cur_msl->memseg_arr;

				tmp = rte_fbarray_get(arr, j);
				if (free_seg(tmp, wa->hi, msl_idx,
						start_idx + j)) {
					RTE_LOG(ERR, EAL, "Cannot free page\n");
					continue;
				}

				rte_fbarray_set_free(arr, j);
			}
			/* clear the list */
			if (wa->ms)
				memset(wa->ms, 0, sizeof(*wa->ms) * wa->n_segs);
			return -1;
		}
		if (wa->ms)
			wa->ms[i] = cur;

		rte_fbarray_set_used(&cur_msl->memseg_arr, cur_idx);
	}
out:
	wa->segs_allocated = i;
	if (i > 0)
		cur_msl->version++;
	return 1;
}

struct free_walk_param {
	struct hugepage_info *hi;
	struct rte_memseg *ms;
};
static int
free_seg_walk(const struct rte_memseg_list *msl, void *arg)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct rte_memseg_list *found_msl;
	struct free_walk_param *wa = arg;
	uintptr_t start_addr, end_addr;
	int msl_idx, seg_idx;

	start_addr = (uintptr_t) msl->base_va;
	end_addr = start_addr + msl->memseg_arr.len * (size_t)msl->page_sz;

	if ((uintptr_t)wa->ms->addr < start_addr ||
			(uintptr_t)wa->ms->addr >= end_addr)
		return 0;

	msl_idx = msl - mcfg->memsegs;
	seg_idx = RTE_PTR_DIFF(wa->ms->addr, start_addr) / msl->page_sz;

	/* msl is const */
	found_msl = &mcfg->memsegs[msl_idx];

	found_msl->version++;

	rte_fbarray_set_free(&found_msl->memseg_arr, seg_idx);

	if (free_seg(wa->ms, wa->hi, msl_idx, seg_idx))
		return -1;

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

	ret = memseg_list_walk_thread_unsafe(alloc_seg_walk, &wa);
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

int
eal_memalloc_free_seg_bulk(struct rte_memseg **ms, int n_segs)
{
	int seg, ret = 0;

	/* dynamic free not supported in legacy mode */
	if (internal_config.legacy_mem)
		return -1;

	for (seg = 0; seg < n_segs; seg++) {
		struct rte_memseg *cur = ms[seg];
		struct hugepage_info *hi = NULL;
		struct free_walk_param wa;
		int i, walk_res;

		/* if this page is marked as unfreeable, fail */
		if (cur->flags & RTE_MEMSEG_FLAG_DO_NOT_FREE) {
			RTE_LOG(DEBUG, EAL, "Page is not allowed to be freed\n");
			ret = -1;
			continue;
		}

		memset(&wa, 0, sizeof(wa));

		for (i = 0; i < (int)RTE_DIM(internal_config.hugepage_info);
				i++) {
			hi = &internal_config.hugepage_info[i];
			if (cur->hugepage_sz == hi->hugepage_sz)
				break;
		}
		if (i == (int)RTE_DIM(internal_config.hugepage_info)) {
			RTE_LOG(ERR, EAL, "Can't find relevant hugepage_info entry\n");
			ret = -1;
			continue;
		}

		wa.ms = cur;
		wa.hi = hi;

		walk_res = memseg_list_walk_thread_unsafe(free_seg_walk, &wa);
		if (walk_res == 1)
			continue;
		if (walk_res == 0)
			RTE_LOG(ERR, EAL, "Couldn't find memseg list\n");
		ret = -1;
	}
	return ret;
}

int
eal_memalloc_free_seg(struct rte_memseg *ms)
{
	/* dynamic free not supported in legacy mode */
	if (internal_config.legacy_mem)
		return -1;

	return eal_memalloc_free_seg_bulk(&ms, 1);
}

static int
sync_chunk(struct rte_memseg_list *primary_msl,
		struct rte_memseg_list *local_msl, struct hugepage_info *hi,
		unsigned int msl_idx, bool used, int start, int end)
{
	struct rte_fbarray *l_arr, *p_arr;
	int i, ret, chunk_len, diff_len;

	l_arr = &local_msl->memseg_arr;
	p_arr = &primary_msl->memseg_arr;

	/* we need to aggregate allocations/deallocations into bigger chunks,
	 * as we don't want to spam the user with per-page callbacks.
	 *
	 * to avoid any potential issues, we also want to trigger
	 * deallocation callbacks *before* we actually deallocate
	 * memory, so that the user application could wrap up its use
	 * before it goes away.
	 */

	chunk_len = end - start;

	/* find how many contiguous pages we can map/unmap for this chunk */
	diff_len = used ?
			rte_fbarray_find_contig_free(l_arr, start) :
			rte_fbarray_find_contig_used(l_arr, start);

	/* has to be at least one page */
	if (diff_len < 1)
		return -1;

	diff_len = RTE_MIN(chunk_len, diff_len);

	/* if we are freeing memory, notify the application */
	if (!used) {
		struct rte_memseg *ms;
		void *start_va;
		size_t len, page_sz;

		ms = rte_fbarray_get(l_arr, start);
		start_va = ms->addr;
		page_sz = (size_t)primary_msl->page_sz;
		len = page_sz * diff_len;

		eal_memalloc_mem_event_notify(RTE_MEM_EVENT_FREE,
				start_va, len);
	}

	for (i = 0; i < diff_len; i++) {
		struct rte_memseg *p_ms, *l_ms;
		int seg_idx = start + i;

		l_ms = rte_fbarray_get(l_arr, seg_idx);
		p_ms = rte_fbarray_get(p_arr, seg_idx);

		if (l_ms == NULL || p_ms == NULL)
			return -1;

		if (used) {
			ret = alloc_seg(l_ms, p_ms->addr,
					p_ms->socket_id, hi,
					msl_idx, seg_idx);
			if (ret < 0)
				return -1;
			rte_fbarray_set_used(l_arr, seg_idx);
		} else {
			ret = free_seg(l_ms, hi, msl_idx, seg_idx);
			rte_fbarray_set_free(l_arr, seg_idx);
			if (ret < 0)
				return -1;
		}
	}

	/* if we just allocated memory, notify the application */
	if (used) {
		struct rte_memseg *ms;
		void *start_va;
		size_t len, page_sz;

		ms = rte_fbarray_get(l_arr, start);
		start_va = ms->addr;
		page_sz = (size_t)primary_msl->page_sz;
		len = page_sz * diff_len;

		eal_memalloc_mem_event_notify(RTE_MEM_EVENT_ALLOC,
				start_va, len);
	}

	/* calculate how much we can advance until next chunk */
	diff_len = used ?
			rte_fbarray_find_contig_used(l_arr, start) :
			rte_fbarray_find_contig_free(l_arr, start);
	ret = RTE_MIN(chunk_len, diff_len);

	return ret;
}

static int
sync_status(struct rte_memseg_list *primary_msl,
		struct rte_memseg_list *local_msl, struct hugepage_info *hi,
		unsigned int msl_idx, bool used)
{
	struct rte_fbarray *l_arr, *p_arr;
	int p_idx, l_chunk_len, p_chunk_len, ret;
	int start, end;

	/* this is a little bit tricky, but the basic idea is - walk both lists
	 * and spot any places where there are discrepancies. walking both lists
	 * and noting discrepancies in a single go is a hard problem, so we do
	 * it in two passes - first we spot any places where allocated segments
	 * mismatch (i.e. ensure that everything that's allocated in the primary
	 * is also allocated in the secondary), and then we do it by looking at
	 * free segments instead.
	 *
	 * we also need to aggregate changes into chunks, as we have to call
	 * callbacks per allocation, not per page.
	 */
	l_arr = &local_msl->memseg_arr;
	p_arr = &primary_msl->memseg_arr;

	if (used)
		p_idx = rte_fbarray_find_next_used(p_arr, 0);
	else
		p_idx = rte_fbarray_find_next_free(p_arr, 0);

	while (p_idx >= 0) {
		int next_chunk_search_idx;

		if (used) {
			p_chunk_len = rte_fbarray_find_contig_used(p_arr,
					p_idx);
			l_chunk_len = rte_fbarray_find_contig_used(l_arr,
					p_idx);
		} else {
			p_chunk_len = rte_fbarray_find_contig_free(p_arr,
					p_idx);
			l_chunk_len = rte_fbarray_find_contig_free(l_arr,
					p_idx);
		}
		/* best case scenario - no differences (or bigger, which will be
		 * fixed during next iteration), look for next chunk
		 */
		if (l_chunk_len >= p_chunk_len) {
			next_chunk_search_idx = p_idx + p_chunk_len;
			goto next_chunk;
		}

		/* if both chunks start at the same point, skip parts we know
		 * are identical, and sync the rest. each call to sync_chunk
		 * will only sync contiguous segments, so we need to call this
		 * until we are sure there are no more differences in this
		 * chunk.
		 */
		start = p_idx + l_chunk_len;
		end = p_idx + p_chunk_len;
		do {
			ret = sync_chunk(primary_msl, local_msl, hi, msl_idx,
					used, start, end);
			start += ret;
		} while (start < end && ret >= 0);
		/* if ret is negative, something went wrong */
		if (ret < 0)
			return -1;

		next_chunk_search_idx = p_idx + p_chunk_len;
next_chunk:
		/* skip to end of this chunk */
		if (used) {
			p_idx = rte_fbarray_find_next_used(p_arr,
					next_chunk_search_idx);
		} else {
			p_idx = rte_fbarray_find_next_free(p_arr,
					next_chunk_search_idx);
		}
	}
	return 0;
}

static int
sync_existing(struct rte_memseg_list *primary_msl,
		struct rte_memseg_list *local_msl, struct hugepage_info *hi,
		unsigned int msl_idx)
{
	int ret;

	/* ensure all allocated space is the same in both lists */
	ret = sync_status(primary_msl, local_msl, hi, msl_idx, true);
	if (ret < 0)
		return -1;

	/* ensure all unallocated space is the same in both lists */
	ret = sync_status(primary_msl, local_msl, hi, msl_idx, false);
	if (ret < 0)
		return -1;

	/* update version number */
	local_msl->version = primary_msl->version;

	return 0;
}

static int
sync_walk(const struct rte_memseg_list *msl, void *arg __rte_unused)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct rte_memseg_list *primary_msl, *local_msl;
	struct hugepage_info *hi = NULL;
	unsigned int i;
	int msl_idx;
	bool new_msl = false;

	msl_idx = msl - mcfg->memsegs;
	primary_msl = &mcfg->memsegs[msl_idx];
	local_msl = &local_memsegs[msl_idx];

	/* check if secondary has this memseg list set up */
	if (local_msl->base_va == NULL) {
		char name[PATH_MAX];
		int ret;
		new_msl = true;

		/* create distinct fbarrays for each secondary */
		snprintf(name, RTE_FBARRAY_NAME_LEN, "%s_%i",
			primary_msl->memseg_arr.name, getpid());

		ret = rte_fbarray_init(&local_msl->memseg_arr, name,
			primary_msl->memseg_arr.len,
			primary_msl->memseg_arr.elt_sz);
		if (ret < 0) {
			RTE_LOG(ERR, EAL, "Cannot initialize local memory map\n");
			return -1;
		}

		local_msl->base_va = primary_msl->base_va;
	}

	for (i = 0; i < RTE_DIM(internal_config.hugepage_info); i++) {
		uint64_t cur_sz =
			internal_config.hugepage_info[i].hugepage_sz;
		uint64_t msl_sz = primary_msl->page_sz;
		if (msl_sz == cur_sz) {
			hi = &internal_config.hugepage_info[i];
			break;
		}
	}
	if (!hi) {
		RTE_LOG(ERR, EAL, "Can't find relevant hugepage_info entry\n");
		return -1;
	}

	/* if versions don't match or if we have just allocated a new
	 * memseg list, synchronize everything
	 */
	if ((new_msl || local_msl->version != primary_msl->version) &&
			sync_existing(primary_msl, local_msl, hi, msl_idx))
		return -1;
	return 0;
}


int
eal_memalloc_sync_with_primary(void)
{
	/* nothing to be done in primary */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		return 0;

	if (memseg_list_walk_thread_unsafe(sync_walk, NULL))
		return -1;
	return 0;
}
