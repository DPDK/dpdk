/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _FD_MAN_H_
#define _FD_MAN_H_
#include <pthread.h>
#include <poll.h>

#define MAX_FDS 1024

typedef void (*fd_cb)(int fd, void *dat, int *remove);

struct fdentry {
	int fd;		/* -1 indicates this entry is empty */
	fd_cb rcb;	/* callback when this fd is readable. */
	fd_cb wcb;	/* callback when this fd is writeable.*/
	void *dat;	/* fd context */
	int busy;	/* whether this entry is being used in cb. */
};

struct fdset {
	struct pollfd rwfds[MAX_FDS];
	struct fdentry fd[MAX_FDS];
	pthread_mutex_t fd_mutex;
	pthread_mutex_t fd_pooling_mutex;
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
};


void fdset_init(struct fdset *pfdset);

int fdset_add(struct fdset *pfdset, int fd,
	fd_cb rcb, fd_cb wcb, void *dat);

void *fdset_del(struct fdset *pfdset, int fd);
int fdset_try_del(struct fdset *pfdset, int fd);

uint32_t fdset_event_dispatch(void *arg);

int fdset_pipe_init(struct fdset *fdset);

void fdset_pipe_uninit(struct fdset *fdset);

void fdset_pipe_notify(struct fdset *fdset);

#endif
