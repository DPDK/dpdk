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

#ifndef _FD_MAN_H_
#define _FD_MAN_H_
#include <stdint.h>
#include <pthread.h>

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
	struct fdentry fd[MAX_FDS];
	pthread_mutex_t fd_mutex;
	int num;	/* current fd number of this fdset */
};


void fdset_init(struct fdset *pfdset);

int fdset_add(struct fdset *pfdset, int fd,
	fd_cb rcb, fd_cb wcb, void *dat);

void *fdset_del(struct fdset *pfdset, int fd);

void fdset_event_dispatch(struct fdset *pfdset);

#endif
