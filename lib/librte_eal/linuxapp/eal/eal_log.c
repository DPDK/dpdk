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

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <syslog.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_spinlock.h>
#include <rte_log.h>

#include "eal_private.h"

/*
 * default log function
 */
static ssize_t
console_log_write(__attribute__((unused)) void *c, const char *buf, size_t size)
{
	char copybuf[BUFSIZ + 1];
	ssize_t ret;
	uint32_t loglevel;

	/* write on stdout */
	ret = fwrite(buf, 1, size, stdout);
	fflush(stdout);

	/* truncate message if too big (should not happen) */
	if (size > BUFSIZ)
		size = BUFSIZ;

	/* Syslog error levels are from 0 to 7, so subtract 1 to convert */
	loglevel = rte_log_cur_msg_loglevel() - 1;
	memcpy(copybuf, buf, size);
	copybuf[size] = '\0';

	/* write on syslog too */
	syslog(loglevel, "%s", copybuf);

	return ret;
}

static cookie_io_functions_t console_log_func = {
	.write = console_log_write,
};

/*
 * set the log to default function, called during eal init process,
 * once memzones are available.
 */
int
rte_eal_log_init(const char *id, int facility)
{
	FILE *log_stream;

	log_stream = fopencookie(NULL, "w+", console_log_func);
	if (log_stream == NULL)
		return -1;

	openlog(id, LOG_NDELAY | LOG_PID, facility);

	eal_log_set_default(log_stream);

	return 0;
}
