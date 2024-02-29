/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_log.h>

#include "fd_man.h"


#define RTE_LOGTYPE_VHOST_FDMAN RTE_LOGTYPE_USER1

#define FDPOLLERR (POLLERR | POLLHUP | POLLNVAL)

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

void
fdset_init(struct fdset *pfdset)
{
	int i;

	if (pfdset == NULL)
		return;

	for (i = 0; i < MAX_FDS; i++) {
		pfdset->fd[i].fd = -1;
		pfdset->fd[i].dat = NULL;
	}
	pfdset->num = 0;
}

/**
 * Register the fd in the fdset with read/write handler and context.
 */
int
fdset_add(struct fdset *pfdset, int fd, fd_cb rcb, fd_cb wcb, void *dat)
{
	int i;

	if (pfdset == NULL || fd == -1)
		return -1;

	pthread_mutex_lock(&pfdset->fd_mutex);
	i = pfdset->num < MAX_FDS ? pfdset->num++ : -1;
	if (i == -1) {
		pthread_mutex_lock(&pfdset->fd_pooling_mutex);
		fdset_shrink_nolock(pfdset);
		pthread_mutex_unlock(&pfdset->fd_pooling_mutex);
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
uint32_t
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

		pthread_mutex_lock(&pfdset->fd_pooling_mutex);
		val = poll(pfdset->rwfds, numfds, 1000 /* millisecs */);
		pthread_mutex_unlock(&pfdset->fd_pooling_mutex);
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
	}

	return 0;
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

void
fdset_pipe_uninit(struct fdset *fdset)
{
	fdset_del(fdset, fdset->u.readfd);
	close(fdset->u.readfd);
	close(fdset->u.writefd);
}

int
fdset_pipe_init(struct fdset *fdset)
{
	int ret;

	if (pipe(fdset->u.pipefd) < 0) {
		RTE_LOG(ERR, VHOST_FDMAN,
			"failed to create pipe for vhost fdset\n");
		return -1;
	}

	ret = fdset_add(fdset, fdset->u.readfd,
			fdset_pipe_read_cb, NULL, fdset);

	if (ret < 0) {
		RTE_LOG(ERR, VHOST_FDMAN,
			"failed to add pipe readfd %d into vhost server fdset\n",
			fdset->u.readfd);

		fdset_pipe_uninit(fdset);
		return -1;
	}

	return 0;
}

void
fdset_pipe_notify(struct fdset *fdset)
{
	int r = write(fdset->u.writefd, "1", 1);
	/*
	 * Just an optimization, we don't care if write() failed
	 * so ignore explicitly its return value to make the
	 * compiler happy
	 */
	RTE_SET_USED(r);
}

void
fdset_pipe_notify_sync(struct fdset *fdset)
{
	pthread_mutex_lock(&fdset->sync_mutex);

	fdset->sync = false;
	fdset_pipe_notify(fdset);

	while (!fdset->sync)
		pthread_cond_wait(&fdset->sync_cond, &fdset->sync_mutex);

	pthread_mutex_unlock(&fdset->sync_mutex);
}
