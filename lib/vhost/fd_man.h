/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _FD_MAN_H_
#define _FD_MAN_H_
#include <pthread.h>
#include <poll.h>
#include <sys/queue.h>

struct fdset;

#define MAX_FDS 1024

typedef void (*fd_cb)(int fd, void *dat, int *close);

struct fdset *fdset_init(const char *name);

int fdset_add(struct fdset *pfdset, int fd,
	fd_cb rcb, fd_cb wcb, void *dat);

void fdset_del(struct fdset *pfdset, int fd);
int fdset_try_del(struct fdset *pfdset, int fd);

#endif
