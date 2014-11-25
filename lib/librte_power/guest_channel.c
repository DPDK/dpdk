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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>


#include <rte_log.h>
#include <rte_config.h>

#include "guest_channel.h"
#include "channel_commands.h"

#define RTE_LOGTYPE_GUEST_CHANNEL RTE_LOGTYPE_USER1

static int global_fds[RTE_MAX_LCORE];

int
guest_channel_host_connect(const char *path, unsigned lcore_id)
{
	int flags, ret;
	struct channel_packet pkt;
	char fd_path[PATH_MAX];
	int fd = -1;

	if (lcore_id >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, GUEST_CHANNEL, "Channel(%u) is out of range 0...%d\n",
				lcore_id, RTE_MAX_LCORE-1);
		return -1;
	}
	/* check if path is already open */
	if (global_fds[lcore_id] != 0) {
		RTE_LOG(ERR, GUEST_CHANNEL, "Channel(%u) is already open with fd %d\n",
				lcore_id, global_fds[lcore_id]);
		return -1;
	}

	snprintf(fd_path, PATH_MAX, "%s.%u", path, lcore_id);
	RTE_LOG(INFO, GUEST_CHANNEL, "Opening channel '%s' for lcore %u\n",
			fd_path, lcore_id);
	fd = open(fd_path, O_RDWR);
	if (fd < 0) {
		RTE_LOG(ERR, GUEST_CHANNEL, "Unable to to connect to '%s' with error "
				"%s\n", fd_path, strerror(errno));
		return -1;
	}

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		RTE_LOG(ERR, GUEST_CHANNEL, "Failed on fcntl get flags for file %s\n",
				fd_path);
		goto error;
	}

	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0) {
		RTE_LOG(ERR, GUEST_CHANNEL, "Failed on setting non-blocking mode for "
				"file %s", fd_path);
		goto error;
	}
	/* QEMU needs a delay after connection */
	sleep(1);

	/* Send a test packet, this command is ignored by the host, but a successful
	 * send indicates that the host endpoint is monitoring.
	 */
	pkt.command = CPU_POWER_CONNECT;
	global_fds[lcore_id] = fd;
	ret = guest_channel_send_msg(&pkt, lcore_id);
	if (ret != 0) {
		RTE_LOG(ERR, GUEST_CHANNEL, "Error on channel '%s' communications "
				"test: %s\n", fd_path, strerror(ret));
		goto error;
	}
	RTE_LOG(INFO, GUEST_CHANNEL, "Channel '%s' is now connected\n", fd_path);
	return 0;
error:
	close(fd);
	global_fds[lcore_id] = 0;
	return -1;
}

int
guest_channel_send_msg(struct channel_packet *pkt, unsigned lcore_id)
{
	int ret, buffer_len = sizeof(*pkt);
	void *buffer = pkt;

	if (lcore_id >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, GUEST_CHANNEL, "Channel(%u) is out of range 0...%d\n",
				lcore_id, RTE_MAX_LCORE-1);
		return -1;
	}

	if (global_fds[lcore_id] == 0) {
		RTE_LOG(ERR, GUEST_CHANNEL, "Channel is not connected\n");
		return -1;
	}
	while (buffer_len > 0) {
		ret = write(global_fds[lcore_id], buffer, buffer_len);
		if (ret == buffer_len)
			return 0;
		if (ret == -1) {
			if (errno == EINTR)
				continue;
			return errno;
		}
		buffer = (char *)buffer + ret;
		buffer_len -= ret;
	}
	return 0;
}

void
guest_channel_host_disconnect(unsigned lcore_id)
{
	if (lcore_id >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, GUEST_CHANNEL, "Channel(%u) is out of range 0...%d\n",
				lcore_id, RTE_MAX_LCORE-1);
		return;
	}
	if (global_fds[lcore_id] == 0)
		return;
	close(global_fds[lcore_id]);
	global_fds[lcore_id] = 0;
}
