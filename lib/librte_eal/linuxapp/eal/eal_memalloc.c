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

/* for single-file segments, we need some kind of mechanism to keep track of
 * which hugepages can be freed back to the system, and which cannot. we cannot
 * use flock() because they don't allow locking parts of a file, and we cannot
 * use fcntl() due to issues with their semantics, so we will have to rely on a
 * bunch of lockfiles for each page.
 *
 * we cannot know how many pages a system will have in advance, but we do know
 * that they come in lists, and we know lengths of these lists. so, simply store
 * a malloc'd array of fd's indexed by list and segment index.
 *
 * they will be initialized at startup, and filled as we allocate/deallocate
 * segments. also, use this to track memseg list proper fd.
 */
static struct {
	int *fds; /**< dynamically allocated array of segment lock fd's */
	int memseg_list_fd; /**< memseg list fd */
	int len; /**< total length of the array */
	int count; /**< entries used in an array */
} lock_fds[RTE_MAX_MEMSEG_LISTS];

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
restore_numa(int *oldpolicy, struct bitmask *oldmask)
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

/* returns 1 on successful lock, 0 on unsuccessful lock, -1 on error */
static int lock(int fd, int type)
{
	int ret;

	/* flock may be interrupted */
	do {
		ret = flock(fd, type | LOCK_NB);
	} while (ret && errno == EINTR);

	if (ret && errno == EWOULDBLOCK) {
		/* couldn't lock */
		return 0;
	} else if (ret) {
		RTE_LOG(ERR, EAL, "%s(): error calling flock(): %s\n",
			__func__, strerror(errno));
		return -1;
	}
	/* lock was successful */
	return 1;
}

static int get_segment_lock_fd(int list_idx, int seg_idx)
{
	char path[PATH_MAX] = {0};
	int fd;

	if (list_idx < 0 || list_idx >= (int)RTE_DIM(lock_fds))
		return -1;
	if (seg_idx < 0 || seg_idx >= lock_fds[list_idx].len)
		return -1;

	fd = lock_fds[list_idx].fds[seg_idx];
	/* does this lock already exist? */
	if (fd >= 0)
		return fd;

	eal_get_hugefile_lock_path(path, sizeof(path),
			list_idx * RTE_MAX_MEMSEG_PER_LIST + seg_idx);

	fd = open(path, O_CREAT | O_RDWR, 0660);
	if (fd < 0) {
		RTE_LOG(ERR, EAL, "%s(): error creating lockfile '%s': %s\n",
			__func__, path, strerror(errno));
		return -1;
	}
	/* take out a read lock */
	if (lock(fd, LOCK_SH) != 1) {
		RTE_LOG(ERR, EAL, "%s(): failed to take out a readlock on '%s': %s\n",
			__func__, path, strerror(errno));
		close(fd);
		return -1;
	}
	/* store it for future reference */
	lock_fds[list_idx].fds[seg_idx] = fd;
	lock_fds[list_idx].count++;
	return fd;
}

static int unlock_segment(int list_idx, int seg_idx)
{
	int fd, ret;

	if (list_idx < 0 || list_idx >= (int)RTE_DIM(lock_fds))
		return -1;
	if (seg_idx < 0 || seg_idx >= lock_fds[list_idx].len)
		return -1;

	fd = lock_fds[list_idx].fds[seg_idx];

	/* upgrade lock to exclusive to see if we can remove the lockfile */
	ret = lock(fd, LOCK_EX);
	if (ret == 1) {
		/* we've succeeded in taking exclusive lock, this lockfile may
		 * be removed.
		 */
		char path[PATH_MAX] = {0};
		eal_get_hugefile_lock_path(path, sizeof(path),
				list_idx * RTE_MAX_MEMSEG_PER_LIST + seg_idx);
		if (unlink(path)) {
			RTE_LOG(ERR, EAL, "%s(): error removing lockfile '%s': %s\n",
					__func__, path, strerror(errno));
		}
	}
	/* we don't want to leak the fd, so even if we fail to lock, close fd
	 * and remove it from list anyway.
	 */
	close(fd);
	lock_fds[list_idx].fds[seg_idx] = -1;
	lock_fds[list_idx].count--;

	if (ret < 0)
		return -1;
	return 0;
}

static int
get_seg_fd(char *path, int buflen, struct hugepage_info *hi,
		unsigned int list_idx, unsigned int seg_idx)
{
	int fd;

	if (internal_config.single_file_segments) {
		/* create a hugepage file path */
		eal_get_hugefile_path(path, buflen, hi->hugedir, list_idx);

		fd = lock_fds[list_idx].memseg_list_fd;

		if (fd < 0) {
			fd = open(path, O_CREAT | O_RDWR, 0600);
			if (fd < 0) {
				RTE_LOG(ERR, EAL, "%s(): open failed: %s\n",
					__func__, strerror(errno));
				return -1;
			}
			/* take out a read lock and keep it indefinitely */
			if (lock(fd, LOCK_SH) < 0) {
				RTE_LOG(ERR, EAL, "%s(): lock failed: %s\n",
					__func__, strerror(errno));
				close(fd);
				return -1;
			}
			lock_fds[list_idx].memseg_list_fd = fd;
		}
	} else {
		/* create a hugepage file path */
		eal_get_hugefile_path(path, buflen, hi->hugedir,
				list_idx * RTE_MAX_MEMSEG_PER_LIST + seg_idx);
		fd = open(path, O_CREAT | O_RDWR, 0600);
		if (fd < 0) {
			RTE_LOG(DEBUG, EAL, "%s(): open failed: %s\n", __func__,
					strerror(errno));
			return -1;
		}
		/* take out a read lock */
		if (lock(fd, LOCK_SH) < 0) {
			RTE_LOG(ERR, EAL, "%s(): lock failed: %s\n",
				__func__, strerror(errno));
			close(fd);
			return -1;
		}
	}
	return fd;
}

static int
resize_hugefile(int fd, char *path, int list_idx, int seg_idx,
		uint64_t fa_offset, uint64_t page_sz, bool grow)
{
	bool again = false;
	do {
		if (fallocate_supported == 0) {
			/* we cannot deallocate memory if fallocate() is not
			 * supported, and hugepage file is already locked at
			 * creation, so no further synchronization needed.
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
		} else {
			int flags = grow ? 0 : FALLOC_FL_PUNCH_HOLE |
					FALLOC_FL_KEEP_SIZE;
			int ret, lock_fd;

			/* if fallocate() is supported, we need to take out a
			 * read lock on allocate (to prevent other processes
			 * from deallocating this page), and take out a write
			 * lock on deallocate (to ensure nobody else is using
			 * this page).
			 *
			 * read locks on page itself are already taken out at
			 * file creation, in get_seg_fd().
			 *
			 * we cannot rely on simple use of flock() call, because
			 * we need to be able to lock a section of the file,
			 * and we cannot use fcntl() locks, because of numerous
			 * problems with their semantics, so we will use
			 * deterministically named lock files for each section
			 * of the file.
			 *
			 * if we're shrinking the file, we want to upgrade our
			 * lock from shared to exclusive.
			 *
			 * lock_fd is an fd for a lockfile, not for the segment
			 * list.
			 */
			lock_fd = get_segment_lock_fd(list_idx, seg_idx);

			if (!grow) {
				/* we are using this lockfile to determine
				 * whether this particular page is locked, as we
				 * are in single file segments mode and thus
				 * cannot use regular flock() to get this info.
				 *
				 * we want to try and take out an exclusive lock
				 * on the lock file to determine if we're the
				 * last ones using this page, and if not, we
				 * won't be shrinking it, and will instead exit
				 * prematurely.
				 */
				ret = lock(lock_fd, LOCK_EX);

				/* drop the lock on the lockfile, so that even
				 * if we couldn't shrink the file ourselves, we
				 * are signalling to other processes that we're
				 * no longer using this page.
				 */
				if (unlock_segment(list_idx, seg_idx))
					RTE_LOG(ERR, EAL, "Could not unlock segment\n");

				/* additionally, if this was the last lock on
				 * this segment list, we can safely close the
				 * page file fd, so that one of the processes
				 * could then delete the file after shrinking.
				 */
				if (ret < 1 && lock_fds[list_idx].count == 0) {
					close(fd);
					lock_fds[list_idx].memseg_list_fd = -1;
				}

				if (ret < 0) {
					RTE_LOG(ERR, EAL, "Could not lock segment\n");
					return -1;
				}
				if (ret == 0)
					/* failed to lock, not an error. */
					return 0;
			}

			/* grow or shrink the file */
			ret = fallocate(fd, flags, fa_offset, page_sz);

			if (ret < 0) {
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

				/* we've grew/shrunk the file, and we hold an
				 * exclusive lock now. check if there are no
				 * more segments active in this segment list,
				 * and remove the file if there aren't.
				 */
				if (lock_fds[list_idx].count == 0) {
					if (unlink(path))
						RTE_LOG(ERR, EAL, "%s(): unlinking '%s' failed: %s\n",
							__func__, path,
							strerror(errno));
					close(fd);
					lock_fds[list_idx].memseg_list_fd = -1;
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

	/* takes out a read lock on segment or segment list */
	fd = get_seg_fd(path, sizeof(path), hi, list_idx, seg_idx);
	if (fd < 0) {
		RTE_LOG(ERR, EAL, "Couldn't get fd on hugepage file\n");
		return -1;
	}

	alloc_sz = hi->hugepage_sz;
	if (internal_config.single_file_segments) {
		map_offset = seg_idx * alloc_sz;
		ret = resize_hugefile(fd, path, list_idx, seg_idx, map_offset,
				alloc_sz, true);
		if (ret < 0)
			goto resized;
	} else {
		map_offset = 0;
		if (ftruncate(fd, alloc_sz) < 0) {
			RTE_LOG(DEBUG, EAL, "%s(): ftruncate() failed: %s\n",
				__func__, strerror(errno));
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
		munmap(va, alloc_sz);
		goto resized;
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

	/* we need to trigger a write to the page to enforce page fault and
	 * ensure that page is accessible to us, but we can't overwrite value
	 * that is already there, so read the old value, and write itback.
	 * kernel populates the page with zeroes initially.
	 */
	*(volatile int *)addr = *(volatile int *)addr;

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
		resize_hugefile(fd, path, list_idx, seg_idx, map_offset,
				alloc_sz, false);
		/* ignore failure, can't make it any worse */
	} else {
		/* only remove file if we can take out a write lock */
		if (lock(fd, LOCK_EX) == 1)
			unlink(path);
		close(fd);
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

	/* if we are not in single file segments mode, we're going to unmap the
	 * segment and thus drop the lock on original fd, but hugepage dir is
	 * now locked so we can take out another one without races.
	 */
	fd = get_seg_fd(path, sizeof(path), hi, list_idx, seg_idx);
	if (fd < 0)
		return -1;

	if (internal_config.single_file_segments) {
		map_offset = seg_idx * ms->len;
		if (resize_hugefile(fd, path, list_idx, seg_idx, map_offset,
				ms->len, false))
			return -1;
		ret = 0;
	} else {
		/* if we're able to take out a write lock, we're the last one
		 * holding onto this page.
		 */
		ret = lock(fd, LOCK_EX);
		if (ret >= 0) {
			/* no one else is using this page */
			if (ret == 1)
				unlink(path);
		}
		/* closing fd will drop the lock */
		close(fd);
	}

	memset(ms, 0, sizeof(*ms));

	return ret < 0 ? -1 : 0;
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
	int cur_idx, start_idx, j, dir_fd = -1;
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

	/* do not allow any page allocations during the time we're allocating,
	 * because file creation and locking operations are not atomic,
	 * and we might be the first or the last ones to use a particular page,
	 * so we need to ensure atomicity of every operation.
	 *
	 * during init, we already hold a write lock, so don't try to take out
	 * another one.
	 */
	if (wa->hi->lock_descriptor == -1) {
		dir_fd = open(wa->hi->hugedir, O_RDONLY);
		if (dir_fd < 0) {
			RTE_LOG(ERR, EAL, "%s(): Cannot open '%s': %s\n",
				__func__, wa->hi->hugedir, strerror(errno));
			return -1;
		}
		/* blocking writelock */
		if (flock(dir_fd, LOCK_EX)) {
			RTE_LOG(ERR, EAL, "%s(): Cannot lock '%s': %s\n",
				__func__, wa->hi->hugedir, strerror(errno));
			close(dir_fd);
			return -1;
		}
	}

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
				rte_fbarray_set_free(arr, j);

				/* free_seg may attempt to create a file, which
				 * may fail.
				 */
				if (free_seg(tmp, wa->hi, msl_idx, j))
					RTE_LOG(DEBUG, EAL, "Cannot free page\n");
			}
			/* clear the list */
			if (wa->ms)
				memset(wa->ms, 0, sizeof(*wa->ms) * wa->n_segs);

			if (dir_fd >= 0)
				close(dir_fd);
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
	if (dir_fd >= 0)
		close(dir_fd);
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
	int msl_idx, seg_idx, ret, dir_fd = -1;

	start_addr = (uintptr_t) msl->base_va;
	end_addr = start_addr + msl->memseg_arr.len * (size_t)msl->page_sz;

	if ((uintptr_t)wa->ms->addr < start_addr ||
			(uintptr_t)wa->ms->addr >= end_addr)
		return 0;

	msl_idx = msl - mcfg->memsegs;
	seg_idx = RTE_PTR_DIFF(wa->ms->addr, start_addr) / msl->page_sz;

	/* msl is const */
	found_msl = &mcfg->memsegs[msl_idx];

	/* do not allow any page allocations during the time we're freeing,
	 * because file creation and locking operations are not atomic,
	 * and we might be the first or the last ones to use a particular page,
	 * so we need to ensure atomicity of every operation.
	 *
	 * during init, we already hold a write lock, so don't try to take out
	 * another one.
	 */
	if (wa->hi->lock_descriptor == -1) {
		dir_fd = open(wa->hi->hugedir, O_RDONLY);
		if (dir_fd < 0) {
			RTE_LOG(ERR, EAL, "%s(): Cannot open '%s': %s\n",
				__func__, wa->hi->hugedir, strerror(errno));
			return -1;
		}
		/* blocking writelock */
		if (flock(dir_fd, LOCK_EX)) {
			RTE_LOG(ERR, EAL, "%s(): Cannot lock '%s': %s\n",
				__func__, wa->hi->hugedir, strerror(errno));
			close(dir_fd);
			return -1;
		}
	}

	found_msl->version++;

	rte_fbarray_set_free(&found_msl->memseg_arr, seg_idx);

	ret = free_seg(wa->ms, wa->hi, msl_idx, seg_idx);

	if (dir_fd >= 0)
		close(dir_fd);

	if (ret < 0)
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
		restore_numa(&oldpolicy, oldmask);
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
	int ret, dir_fd;

	/* do not allow any page allocations during the time we're allocating,
	 * because file creation and locking operations are not atomic,
	 * and we might be the first or the last ones to use a particular page,
	 * so we need to ensure atomicity of every operation.
	 */
	dir_fd = open(hi->hugedir, O_RDONLY);
	if (dir_fd < 0) {
		RTE_LOG(ERR, EAL, "%s(): Cannot open '%s': %s\n", __func__,
			hi->hugedir, strerror(errno));
		return -1;
	}
	/* blocking writelock */
	if (flock(dir_fd, LOCK_EX)) {
		RTE_LOG(ERR, EAL, "%s(): Cannot lock '%s': %s\n", __func__,
			hi->hugedir, strerror(errno));
		close(dir_fd);
		return -1;
	}

	/* ensure all allocated space is the same in both lists */
	ret = sync_status(primary_msl, local_msl, hi, msl_idx, true);
	if (ret < 0)
		goto fail;

	/* ensure all unallocated space is the same in both lists */
	ret = sync_status(primary_msl, local_msl, hi, msl_idx, false);
	if (ret < 0)
		goto fail;

	/* update version number */
	local_msl->version = primary_msl->version;

	close(dir_fd);

	return 0;
fail:
	close(dir_fd);
	return -1;
}

static int
sync_walk(const struct rte_memseg_list *msl, void *arg __rte_unused)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct rte_memseg_list *primary_msl, *local_msl;
	struct hugepage_info *hi = NULL;
	unsigned int i;
	int msl_idx;

	msl_idx = msl - mcfg->memsegs;
	primary_msl = &mcfg->memsegs[msl_idx];
	local_msl = &local_memsegs[msl_idx];

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

	/* if versions don't match, synchronize everything */
	if (local_msl->version != primary_msl->version &&
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

static int
secondary_msl_create_walk(const struct rte_memseg_list *msl,
		void *arg __rte_unused)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	struct rte_memseg_list *primary_msl, *local_msl;
	char name[PATH_MAX];
	int msl_idx, ret;

	msl_idx = msl - mcfg->memsegs;
	primary_msl = &mcfg->memsegs[msl_idx];
	local_msl = &local_memsegs[msl_idx];

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

	return 0;
}

static int
secondary_lock_list_create_walk(const struct rte_memseg_list *msl,
		void *arg __rte_unused)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	unsigned int i, len;
	int msl_idx;
	int *data;

	msl_idx = msl - mcfg->memsegs;
	len = msl->memseg_arr.len;

	/* ensure we have space to store lock fd per each possible segment */
	data = malloc(sizeof(int) * len);
	if (data == NULL) {
		RTE_LOG(ERR, EAL, "Unable to allocate space for lock descriptors\n");
		return -1;
	}
	/* set all fd's as invalid */
	for (i = 0; i < len; i++)
		data[i] = -1;

	lock_fds[msl_idx].fds = data;
	lock_fds[msl_idx].len = len;
	lock_fds[msl_idx].count = 0;
	lock_fds[msl_idx].memseg_list_fd = -1;

	return 0;
}

int
eal_memalloc_init(void)
{
	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		if (rte_memseg_list_walk(secondary_msl_create_walk, NULL) < 0)
			return -1;

	/* initialize all of the lock fd lists */
	if (internal_config.single_file_segments)
		if (rte_memseg_list_walk(secondary_lock_list_create_walk, NULL))
			return -1;
	return 0;
}
