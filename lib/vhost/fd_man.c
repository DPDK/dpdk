/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
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

struct fdentry {
	int fd;		/* -1 indicates this entry is empty */
	fd_cb rcb;	/* callback when this fd is readable. */
	fd_cb wcb;	/* callback when this fd is writeable.*/
	void *dat;	/* fd context */
	int busy;	/* whether this entry is being used in cb. */
	LIST_ENTRY(fdentry) next;
};

struct fdset {
	char name[RTE_THREAD_NAME_SIZE];
	int epfd;
	struct fdentry fd[MAX_FDS];
	LIST_HEAD(, fdentry) fdlist;
	int next_free_idx;
	rte_thread_t tid;
	pthread_mutex_t fd_mutex;
	bool destroy;
};

#define MAX_FDSETS 8

static struct fdset *fdsets[MAX_FDSETS];
static pthread_mutex_t fdsets_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint32_t fdset_event_dispatch(void *arg);

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
		VHOST_FDMAN_LOG(ERR, "failed to alloc fdset %s", name);
		goto err_unlock;
	}

	rte_strscpy(fdset->name, name, RTE_THREAD_NAME_SIZE);

	pthread_mutex_init(&fdset->fd_mutex, NULL);

	for (i = 0; i < (int)RTE_DIM(fdset->fd); i++) {
		fdset->fd[i].fd = -1;
		fdset->fd[i].dat = NULL;
	}
	LIST_INIT(&fdset->fdlist);

	/*
	 * Any non-zero value would work (see man epoll_create),
	 * but pass MAX_FDS for consistency.
	 */
	fdset->epfd = epoll_create(MAX_FDS);
	if (fdset->epfd < 0) {
		VHOST_FDMAN_LOG(ERR, "failed to create epoll for %s fdset", name);
		goto err_free;
	}

	if (rte_thread_create_internal_control(&fdset->tid, fdset->name,
					fdset_event_dispatch, fdset)) {
		VHOST_FDMAN_LOG(ERR, "Failed to create %s event dispatch thread",
				fdset->name);
		goto err_epoll;
	}

	if (fdset_insert(fdset)) {
		VHOST_FDMAN_LOG(ERR, "Failed to insert fdset %s", name);
		goto err_thread;
	}

	pthread_mutex_unlock(&fdsets_mutex);

	return fdset;

err_thread:
	fdset->destroy = true;
	rte_thread_join(fdset->tid, &val);
err_epoll:
	close(fdset->epfd);
err_free:
	rte_free(fdset);
err_unlock:
	pthread_mutex_unlock(&fdsets_mutex);

	return NULL;
}

static int
fdset_insert_entry(struct fdset *pfdset, int fd, fd_cb rcb, fd_cb wcb, void *dat)
{
	struct fdentry *pfdentry;

	if (pfdset->next_free_idx >= (int)RTE_DIM(pfdset->fd))
		return -1;

	pfdentry = &pfdset->fd[pfdset->next_free_idx];
	pfdentry->fd  = fd;
	pfdentry->rcb = rcb;
	pfdentry->wcb = wcb;
	pfdentry->dat = dat;

	LIST_INSERT_HEAD(&pfdset->fdlist, pfdentry, next);

	/* Find next free slot */
	pfdset->next_free_idx++;
	for (; pfdset->next_free_idx < (int)RTE_DIM(pfdset->fd); pfdset->next_free_idx++) {
		if (pfdset->fd[pfdset->next_free_idx].fd != -1)
			continue;
		break;
	}

	return 0;
}

static void
fdset_remove_entry(struct fdset *pfdset, struct fdentry *pfdentry)
{
	int entry_idx;

	pfdentry->fd = -1;
	pfdentry->rcb = pfdentry->wcb = NULL;
	pfdentry->dat = NULL;

	entry_idx = pfdentry - pfdset->fd;
	if (entry_idx < pfdset->next_free_idx)
		pfdset->next_free_idx = entry_idx;

	LIST_REMOVE(pfdentry, next);
}

static struct fdentry *
fdset_find_entry_locked(struct fdset *pfdset, int fd)
{
	struct fdentry *pfdentry;

	LIST_FOREACH(pfdentry, &pfdset->fdlist, next) {
		if (pfdentry->fd != fd)
			continue;
		return pfdentry;
	}

	return NULL;
}

/**
 * Register the fd in the fdset with read/write handler and context.
 */
int
fdset_add(struct fdset *pfdset, int fd, fd_cb rcb, fd_cb wcb, void *dat)
{
	struct epoll_event ev;
	struct fdentry *pfdentry;
	int ret = 0;

	if (pfdset == NULL || fd == -1) {
		ret = -1;
		goto out;
	}

	pthread_mutex_lock(&pfdset->fd_mutex);
	ret = fdset_insert_entry(pfdset, fd, rcb, wcb, dat);
	if (ret < 0) {
		VHOST_FDMAN_LOG(ERR, "failed to insert fdset entry");
		pthread_mutex_unlock(&pfdset->fd_mutex);
		goto out;
	}
	pthread_mutex_unlock(&pfdset->fd_mutex);

	ev.events = EPOLLERR;
	ev.events |= rcb ? EPOLLIN : 0;
	ev.events |= wcb ? EPOLLOUT : 0;
	ev.data.fd = fd;

	ret = epoll_ctl(pfdset->epfd, EPOLL_CTL_ADD, fd, &ev);
	if (ret < 0) {
		VHOST_FDMAN_LOG(ERR, "could not add %d fd to %d epfd: %s",
			fd, pfdset->epfd, strerror(errno));
		goto out_remove;
	}

	return 0;
out_remove:
	pthread_mutex_lock(&pfdset->fd_mutex);
	pfdentry = fdset_find_entry_locked(pfdset, fd);
	if (pfdentry)
		fdset_remove_entry(pfdset, pfdentry);
	pthread_mutex_unlock(&pfdset->fd_mutex);
out:
	return ret;
}

static void
fdset_del_locked(struct fdset *pfdset, struct fdentry *pfdentry)
{
	if (epoll_ctl(pfdset->epfd, EPOLL_CTL_DEL, pfdentry->fd, NULL) == -1) {
		if (errno == EBADF) /* File might have already been closed. */
			VHOST_FDMAN_LOG(DEBUG, "could not remove %d fd from %d epfd: %s",
				pfdentry->fd, pfdset->epfd, strerror(errno));
		else
			VHOST_FDMAN_LOG(ERR, "could not remove %d fd from %d epfd: %s",
				pfdentry->fd, pfdset->epfd, strerror(errno));
	}

	fdset_remove_entry(pfdset, pfdentry);
}

void
fdset_del(struct fdset *pfdset, int fd)
{
	struct fdentry *pfdentry;

	if (pfdset == NULL || fd == -1)
		return;

	do {
		pthread_mutex_lock(&pfdset->fd_mutex);
		pfdentry = fdset_find_entry_locked(pfdset, fd);
		if (pfdentry != NULL && pfdentry->busy == 0) {
			fdset_del_locked(pfdset, pfdentry);
			pfdentry = NULL;
		}
		pthread_mutex_unlock(&pfdset->fd_mutex);
	} while (pfdentry != NULL);
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
	struct fdentry *pfdentry;

	if (pfdset == NULL || fd == -1)
		return -2;

	pthread_mutex_lock(&pfdset->fd_mutex);
	pfdentry = fdset_find_entry_locked(pfdset, fd);
	if (pfdentry != NULL && pfdentry->busy != 0) {
		pthread_mutex_unlock(&pfdset->fd_mutex);
		return -1;
	}

	if (pfdentry != NULL)
		fdset_del_locked(pfdset, pfdentry);

	pthread_mutex_unlock(&pfdset->fd_mutex);
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
	fd_cb rcb, wcb;
	void *dat;
	int fd, numfds;
	int close1, close2;
	struct fdset *pfdset = arg;

	if (pfdset == NULL)
		return 0;

	while (1) {
		struct epoll_event events[MAX_FDS];
		struct fdentry *pfdentry;

		numfds = epoll_wait(pfdset->epfd, events, RTE_DIM(events), 1000);
		if (numfds < 0)
			continue;

		for (i = 0; i < numfds; i++) {
			pthread_mutex_lock(&pfdset->fd_mutex);

			fd = events[i].data.fd;
			pfdentry = fdset_find_entry_locked(pfdset, fd);
			if (pfdentry == NULL) {
				pthread_mutex_unlock(&pfdset->fd_mutex);
				continue;
			}

			close1 = close2 = 0;

			rcb = pfdentry->rcb;
			wcb = pfdentry->wcb;
			dat = pfdentry->dat;
			pfdentry->busy = 1;

			pthread_mutex_unlock(&pfdset->fd_mutex);

			if (rcb && events[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP))
				rcb(fd, dat, &close1);
			if (wcb && events[i].events & (EPOLLOUT | EPOLLERR | EPOLLHUP))
				wcb(fd, dat, &close2);
			pfdentry->busy = 0;
			/*
			 * fdset_del needs to check busy flag.
			 * We don't allow fdset_del to be called in callback
			 * directly.
			 */
			/*
			 * A concurrent fdset_del may have been waiting for the
			 * fdentry not to be busy, so we can't call
			 * fdset_del_locked().
			 */
			if (close1 || close2) {
				fdset_del(pfdset, fd);
				close(fd);
			}
		}

		if (pfdset->destroy)
			break;
	}

	return 0;
}
