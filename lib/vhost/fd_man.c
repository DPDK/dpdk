/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>
#include <rte_thread.h>

#include "fd_man.h"

RTE_LOG_REGISTER_SUFFIX(vhost_fdset_logtype, fdset, INFO);
#define RTE_LOGTYPE_VHOST_FDMAN vhost_fdset_logtype
#define VHOST_FDMAN_LOG(level, ...) \
	RTE_LOG_LINE(level, VHOST_FDMAN, "" __VA_ARGS__)

#define FDPOLLERR (POLLERR | POLLHUP | POLLNVAL)

struct fdentry {
	int fd;		/* -1 indicates this entry is empty */
	fd_cb rcb;	/* callback when this fd is readable. */
	fd_cb wcb;	/* callback when this fd is writeable.*/
	void *dat;	/* fd context */
	int busy;	/* whether this entry is being used in cb. */
};

struct fdset {
	char name[RTE_THREAD_NAME_SIZE];
	struct pollfd rwfds[MAX_FDS];
	struct fdentry fd[MAX_FDS];
	rte_thread_t tid;
	pthread_mutex_t fd_mutex;
	pthread_mutex_t fd_polling_mutex;
	int num;	/* current fd number of this fdset */

	union pipefds {
		struct {
			int pipefd[2];
		};
		struct {
			int readfd;
			int writefd;
		};
	} u;

	pthread_mutex_t sync_mutex;
	pthread_cond_t sync_cond;
	bool sync;
	bool destroy;
};

static int fdset_add_no_sync(struct fdset *pfdset, int fd, fd_cb rcb, fd_cb wcb, void *dat);
static uint32_t fdset_event_dispatch(void *arg);

#define MAX_FDSETS 8

static struct fdset *fdsets[MAX_FDSETS];
static pthread_mutex_t fdsets_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct fdset *
fdset_lookup(const char *name)
{
	int i;

	for (i = 0; i < MAX_FDSETS; i++) {
		struct fdset *fdset = fdsets[i];
		if (fdset == NULL)
			continue;

		if (!strncmp(fdset->name, name, RTE_THREAD_NAME_SIZE))
			return fdset;
	}

	return NULL;
}

static int
fdset_insert(struct fdset *fdset)
{
	int i;

	for (i = 0; i < MAX_FDSETS; i++) {
		if (fdsets[i] == NULL) {
			fdsets[i] = fdset;
			return 0;
		}
	}

	return -1;
}

static void
fdset_pipe_read_cb(int readfd, void *dat,
		   int *remove __rte_unused)
{
	char charbuf[16];
	struct fdset *fdset = dat;
	int r = read(readfd, charbuf, sizeof(charbuf));
	/*
	 * Just an optimization, we don't care if read() failed
	 * so ignore explicitly its return value to make the
	 * compiler happy
	 */
	RTE_SET_USED(r);

	pthread_mutex_lock(&fdset->sync_mutex);
	fdset->sync = true;
	pthread_cond_broadcast(&fdset->sync_cond);
	pthread_mutex_unlock(&fdset->sync_mutex);
}

static void
fdset_pipe_uninit(struct fdset *fdset)
{
	fdset_del(fdset, fdset->u.readfd);
	close(fdset->u.readfd);
	fdset->u.readfd = -1;
	close(fdset->u.writefd);
	fdset->u.writefd = -1;
}

static int
fdset_pipe_init(struct fdset *fdset)
{
	int ret;

	pthread_mutex_init(&fdset->sync_mutex, NULL);
	pthread_cond_init(&fdset->sync_cond, NULL);

	if (pipe(fdset->u.pipefd) < 0) {
		VHOST_FDMAN_LOG(ERR,
			"failed to create pipe for vhost fdset");
		return -1;
	}

	ret = fdset_add_no_sync(fdset, fdset->u.readfd,
			fdset_pipe_read_cb, NULL, fdset);
	if (ret < 0) {
		VHOST_FDMAN_LOG(ERR,
			"failed to add pipe readfd %d into vhost server fdset",
			fdset->u.readfd);

		fdset_pipe_uninit(fdset);
		return -1;
	}

	return 0;
}

static void
fdset_sync(struct fdset *fdset)
{
	int ret;

	pthread_mutex_lock(&fdset->sync_mutex);

	fdset->sync = false;
	ret = write(fdset->u.writefd, "1", 1);
	if (ret < 0) {
		VHOST_FDMAN_LOG(ERR,
			"Failed to write to notification pipe: %s",
			strerror(errno));
		goto out_unlock;
	}

	while (!fdset->sync)
		pthread_cond_wait(&fdset->sync_cond, &fdset->sync_mutex);

out_unlock:
	pthread_mutex_unlock(&fdset->sync_mutex);
}

static int
get_last_valid_idx(struct fdset *pfdset, int last_valid_idx)
{
	int i;

	for (i = last_valid_idx; i >= 0 && pfdset->fd[i].fd == -1; i--)
		;

	return i;
}

static void
fdset_move(struct fdset *pfdset, int dst, int src)
{
	pfdset->fd[dst]    = pfdset->fd[src];
	pfdset->rwfds[dst] = pfdset->rwfds[src];
}

static void
fdset_shrink_nolock(struct fdset *pfdset)
{
	int i;
	int last_valid_idx = get_last_valid_idx(pfdset, pfdset->num - 1);

	for (i = 0; i < last_valid_idx; i++) {
		if (pfdset->fd[i].fd != -1)
			continue;

		fdset_move(pfdset, i, last_valid_idx);
		last_valid_idx = get_last_valid_idx(pfdset, last_valid_idx - 1);
	}
	pfdset->num = last_valid_idx + 1;
}

/*
 * Find deleted fd entries and remove them
 */
static void
fdset_shrink(struct fdset *pfdset)
{
	pthread_mutex_lock(&pfdset->fd_mutex);
	fdset_shrink_nolock(pfdset);
	pthread_mutex_unlock(&pfdset->fd_mutex);
}

/**
 * Returns the index in the fdset for a given fd.
 * @return
 *   index for the fd, or -1 if fd isn't in the fdset.
 */
static int
fdset_find_fd(struct fdset *pfdset, int fd)
{
	int i;

	for (i = 0; i < pfdset->num && pfdset->fd[i].fd != fd; i++)
		;

	return i == pfdset->num ? -1 : i;
}

static void
fdset_add_fd(struct fdset *pfdset, int idx, int fd,
	fd_cb rcb, fd_cb wcb, void *dat)
{
	struct fdentry *pfdentry = &pfdset->fd[idx];
	struct pollfd *pfd = &pfdset->rwfds[idx];

	pfdentry->fd  = fd;
	pfdentry->rcb = rcb;
	pfdentry->wcb = wcb;
	pfdentry->dat = dat;

	pfd->fd = fd;
	pfd->events  = rcb ? POLLIN : 0;
	pfd->events |= wcb ? POLLOUT : 0;
	pfd->revents = 0;
}

struct fdset *
fdset_init(const char *name)
{
	struct fdset *fdset;
	uint32_t val;
	int i;

	pthread_mutex_lock(&fdsets_mutex);
	fdset = fdset_lookup(name);
	if (fdset) {
		pthread_mutex_unlock(&fdsets_mutex);
		return fdset;
	}

	fdset = rte_zmalloc(NULL, sizeof(*fdset), 0);
	if (!fdset) {
		VHOST_FDMAN_LOG(ERR, "Failed to alloc fdset %s", name);
		goto err_unlock;
	}

	rte_strscpy(fdset->name, name, RTE_THREAD_NAME_SIZE);

	pthread_mutex_init(&fdset->fd_mutex, NULL);
	pthread_mutex_init(&fdset->fd_polling_mutex, NULL);

	for (i = 0; i < MAX_FDS; i++) {
		fdset->fd[i].fd = -1;
		fdset->fd[i].dat = NULL;
	}
	fdset->num = 0;

	if (fdset_pipe_init(fdset)) {
		VHOST_FDMAN_LOG(ERR, "Failed to init pipe for %s", name);
		goto err_free;
	}

	if (rte_thread_create_internal_control(&fdset->tid, fdset->name,
					fdset_event_dispatch, fdset)) {
		VHOST_FDMAN_LOG(ERR, "Failed to create %s event dispatch thread",
				fdset->name);
		goto err_pipe;
	}

	if (fdset_insert(fdset)) {
		VHOST_FDMAN_LOG(ERR, "Failed to insert fdset %s", name);
		goto err_thread;
	}

	pthread_mutex_unlock(&fdsets_mutex);

	return fdset;

err_thread:
	fdset->destroy = true;
	fdset_sync(fdset);
	rte_thread_join(fdset->tid, &val);
err_pipe:
	fdset_pipe_uninit(fdset);
err_free:
	rte_free(fdset);
err_unlock:
	pthread_mutex_unlock(&fdsets_mutex);

	return NULL;
}

/**
 * Register the fd in the fdset with read/write handler and context.
 */
static int
fdset_add_no_sync(struct fdset *pfdset, int fd, fd_cb rcb, fd_cb wcb, void *dat)
{
	int i;

	if (pfdset == NULL || fd == -1)
		return -1;

	pthread_mutex_lock(&pfdset->fd_mutex);
	i = pfdset->num < MAX_FDS ? pfdset->num++ : -1;
	if (i == -1) {
		pthread_mutex_lock(&pfdset->fd_polling_mutex);
		fdset_shrink_nolock(pfdset);
		pthread_mutex_unlock(&pfdset->fd_polling_mutex);
		i = pfdset->num < MAX_FDS ? pfdset->num++ : -1;
		if (i == -1) {
			pthread_mutex_unlock(&pfdset->fd_mutex);
			return -2;
		}
	}

	fdset_add_fd(pfdset, i, fd, rcb, wcb, dat);
	pthread_mutex_unlock(&pfdset->fd_mutex);

	return 0;
}

int
fdset_add(struct fdset *pfdset, int fd, fd_cb rcb, fd_cb wcb, void *dat)
{
	int ret;

	ret = fdset_add_no_sync(pfdset, fd, rcb, wcb, dat);
	if (ret < 0)
		return ret;

	fdset_sync(pfdset);

	return 0;
}

/**
 *  Unregister the fd from the fdset.
 *  Returns context of a given fd or NULL.
 */
void *
fdset_del(struct fdset *pfdset, int fd)
{
	int i;
	void *dat = NULL;

	if (pfdset == NULL || fd == -1)
		return NULL;

	do {
		pthread_mutex_lock(&pfdset->fd_mutex);

		i = fdset_find_fd(pfdset, fd);
		if (i != -1 && pfdset->fd[i].busy == 0) {
			/* busy indicates r/wcb is executing! */
			dat = pfdset->fd[i].dat;
			pfdset->fd[i].fd = -1;
			pfdset->fd[i].rcb = pfdset->fd[i].wcb = NULL;
			pfdset->fd[i].dat = NULL;
			i = -1;
		}
		pthread_mutex_unlock(&pfdset->fd_mutex);
	} while (i != -1);

	fdset_sync(pfdset);

	return dat;
}

/**
 *  Unregister the fd from the fdset.
 *
 *  If parameters are invalid, return directly -2.
 *  And check whether fd is busy, if yes, return -1.
 *  Otherwise, try to delete the fd from fdset and
 *  return true.
 */
int
fdset_try_del(struct fdset *pfdset, int fd)
{
	int i;

	if (pfdset == NULL || fd == -1)
		return -2;

	pthread_mutex_lock(&pfdset->fd_mutex);
	i = fdset_find_fd(pfdset, fd);
	if (i != -1 && pfdset->fd[i].busy) {
		pthread_mutex_unlock(&pfdset->fd_mutex);
		return -1;
	}

	if (i != -1) {
		pfdset->fd[i].fd = -1;
		pfdset->fd[i].rcb = pfdset->fd[i].wcb = NULL;
		pfdset->fd[i].dat = NULL;
	}

	pthread_mutex_unlock(&pfdset->fd_mutex);

	fdset_sync(pfdset);

	return 0;
}

/**
 * This functions runs in infinite blocking loop until there is no fd in
 * pfdset. It calls corresponding r/w handler if there is event on the fd.
 *
 * Before the callback is called, we set the flag to busy status; If other
 * thread(now rte_vhost_driver_unregister) calls fdset_del concurrently, it
 * will wait until the flag is reset to zero(which indicates the callback is
 * finished), then it could free the context after fdset_del.
 */
static uint32_t
fdset_event_dispatch(void *arg)
{
	int i;
	struct pollfd *pfd;
	struct fdentry *pfdentry;
	fd_cb rcb, wcb;
	void *dat;
	int fd, numfds;
	int remove1, remove2;
	int need_shrink;
	struct fdset *pfdset = arg;
	int val;

	if (pfdset == NULL)
		return 0;

	while (1) {

		/*
		 * When poll is blocked, other threads might unregister
		 * listenfds from and register new listenfds into fdset.
		 * When poll returns, the entries for listenfds in the fdset
		 * might have been updated. It is ok if there is unwanted call
		 * for new listenfds.
		 */
		pthread_mutex_lock(&pfdset->fd_mutex);
		numfds = pfdset->num;
		pthread_mutex_unlock(&pfdset->fd_mutex);

		pthread_mutex_lock(&pfdset->fd_polling_mutex);
		val = poll(pfdset->rwfds, numfds, 1000 /* millisecs */);
		pthread_mutex_unlock(&pfdset->fd_polling_mutex);
		if (val < 0)
			continue;

		need_shrink = 0;
		for (i = 0; i < numfds; i++) {
			pthread_mutex_lock(&pfdset->fd_mutex);

			pfdentry = &pfdset->fd[i];
			fd = pfdentry->fd;
			pfd = &pfdset->rwfds[i];

			if (fd < 0) {
				need_shrink = 1;
				pthread_mutex_unlock(&pfdset->fd_mutex);
				continue;
			}

			if (!pfd->revents) {
				pthread_mutex_unlock(&pfdset->fd_mutex);
				continue;
			}

			remove1 = remove2 = 0;

			rcb = pfdentry->rcb;
			wcb = pfdentry->wcb;
			dat = pfdentry->dat;
			pfdentry->busy = 1;

			pthread_mutex_unlock(&pfdset->fd_mutex);

			if (rcb && pfd->revents & (POLLIN | FDPOLLERR))
				rcb(fd, dat, &remove1);
			if (wcb && pfd->revents & (POLLOUT | FDPOLLERR))
				wcb(fd, dat, &remove2);
			pfdentry->busy = 0;
			/*
			 * fdset_del needs to check busy flag.
			 * We don't allow fdset_del to be called in callback
			 * directly.
			 */
			/*
			 * When we are to clean up the fd from fdset,
			 * because the fd is closed in the cb,
			 * the old fd val could be reused by when creates new
			 * listen fd in another thread, we couldn't call
			 * fdset_del.
			 */
			if (remove1 || remove2) {
				pfdentry->fd = -1;
				need_shrink = 1;
			}
		}

		if (need_shrink)
			fdset_shrink(pfdset);

		if (pfdset->destroy)
			break;
	}

	return 0;
}
