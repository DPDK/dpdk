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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <rte_log.h>

#include "fd_man.h"

/**
 * Returns the index in the fdset for a given fd.
 * If fd is -1, it means to search for a free entry.
 * @return
 *   index for the fd, or -1 if fd isn't in the fdset.
 */
static int
fdset_find_fd(struct fdset *pfdset, int fd)
{
	int i;

	if (pfdset == NULL)
		return -1;

	for (i = 0; i < MAX_FDS && pfdset->fd[i].fd != fd; i++)
		;

	return i ==  MAX_FDS ? -1 : i;
}

static int
fdset_find_free_slot(struct fdset *pfdset)
{
	return fdset_find_fd(pfdset, -1);
}

static void
fdset_add_fd(struct fdset  *pfdset, int idx, int fd,
	fd_cb rcb, fd_cb wcb, void *dat)
{
	struct fdentry *pfdentry;

	if (pfdset == NULL || idx >= MAX_FDS)
		return;

	pfdentry = &pfdset->fd[idx];
	pfdentry->fd = fd;
	pfdentry->rcb = rcb;
	pfdentry->wcb = wcb;
	pfdentry->dat = dat;
}

/**
 * Fill the read/write fd_set with the fds in the fdset.
 * @return
 *  the maximum fds filled in the read/write fd_set.
 */
static int
fdset_fill(fd_set *rfset, fd_set *wfset, struct fdset *pfdset)
{
	struct fdentry *pfdentry;
	int i, maxfds = -1;
	int num = MAX_FDS;

	if (pfdset == NULL)
		return -1;

	for (i = 0; i < num; i++) {
		pfdentry = &pfdset->fd[i];
		if (pfdentry->fd != -1) {
			int added = 0;
			if (pfdentry->rcb && rfset) {
				FD_SET(pfdentry->fd, rfset);
				added = 1;
			}
			if (pfdentry->wcb && wfset) {
				FD_SET(pfdentry->fd, wfset);
				added = 1;
			}
			if (added)
				maxfds = pfdentry->fd < maxfds ?
					maxfds : pfdentry->fd;
		}
	}
	return maxfds;
}

void
fdset_init(struct fdset *pfdset)
{
	int i;

	if (pfdset == NULL)
		return;

	for (i = 0; i < MAX_FDS; i++)
		pfdset->fd[i].fd = -1;
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

	/* Find a free slot in the list. */
	i = fdset_find_free_slot(pfdset);
	if (i == -1)
		return -2;

	fdset_add_fd(pfdset, i, fd, rcb, wcb, dat);
	pfdset->num++;

	return 0;
}

/**
 *  Unregister the fd from the fdset.
 */
void
fdset_del(struct fdset *pfdset, int fd)
{
	int i;

	i = fdset_find_fd(pfdset, fd);
	if (i != -1 && fd != -1) {
		pfdset->fd[i].fd = -1;
		pfdset->fd[i].rcb = pfdset->fd[i].wcb = NULL;
		pfdset->num--;
	}
}

/**
 * This functions runs in infinite blocking loop until there is no fd in
 * pfdset. It calls corresponding r/w handler if there is event on the fd.
 */
void
fdset_event_dispatch(struct fdset *pfdset)
{
	fd_set rfds, wfds;
	int i, maxfds;
	struct fdentry *pfdentry;
	int num = MAX_FDS;

	if (pfdset == NULL)
		return;

	while (1) {
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		maxfds = fdset_fill(&rfds, &wfds, pfdset);
		if (maxfds == -1)
			return;

		select(maxfds + 1, &rfds, &wfds, NULL, NULL);

		for (i = 0; i < num; i++) {
			pfdentry = &pfdset->fd[i];
			if (pfdentry->fd >= 0 && FD_ISSET(pfdentry->fd, &rfds) && pfdentry->rcb)
				pfdentry->rcb(pfdentry->fd, pfdentry->dat);
			if (pfdentry->fd >= 0 && FD_ISSET(pfdentry->fd, &wfds) && pfdentry->wcb)
				pfdentry->wcb(pfdentry->fd, pfdentry->dat);
		}
	}
}
