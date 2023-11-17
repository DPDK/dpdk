/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <glob.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <poll.h>


#include <rte_log.h>
#include <rte_power.h>

#include "guest_channel.h"

RTE_LOG_REGISTER_SUFFIX(guest_channel_logtype, guest_channel, INFO);
#define RTE_LOGTYPE_GUEST_CHANNEL guest_channel_logtype
#define GUEST_CHANNEL_LOG(level, ...) \
	RTE_LOG_LINE(level, GUEST_CHANNEL, "" __VA_ARGS__)

/* Timeout for incoming message in milliseconds. */
#define TIMEOUT 10

static int global_fds[RTE_MAX_LCORE] = { [0 ... RTE_MAX_LCORE-1] = -1 };

int
guest_channel_host_check_exists(const char *path)
{
	char glob_path[PATH_MAX];
	glob_t g;
	int ret;

	/* we cannot know in advance which cores have VM channels, so glob */
	snprintf(glob_path, PATH_MAX, "%s.*", path);

	ret = glob(glob_path, GLOB_NOSORT, NULL, &g);
	if (ret != 0) {
		/* couldn't read anything */
		ret = 0;
		goto out;
	}

	/* do we have at least one match? */
	ret = g.gl_pathc > 0;

out:
	globfree(&g);
	return ret;
}

int
guest_channel_host_connect(const char *path, unsigned int lcore_id)
{
	int flags, ret;
	struct rte_power_channel_packet pkt;
	char fd_path[PATH_MAX];
	int fd = -1;

	if (lcore_id >= RTE_MAX_LCORE) {
		GUEST_CHANNEL_LOG(ERR, "Channel(%u) is out of range 0...%d",
				lcore_id, RTE_MAX_LCORE-1);
		return -1;
	}
	/* check if path is already open */
	if (global_fds[lcore_id] != -1) {
		GUEST_CHANNEL_LOG(ERR, "Channel(%u) is already open with fd %d",
				lcore_id, global_fds[lcore_id]);
		return -1;
	}

	snprintf(fd_path, PATH_MAX, "%s.%u", path, lcore_id);
	GUEST_CHANNEL_LOG(INFO, "Opening channel '%s' for lcore %u",
			fd_path, lcore_id);
	fd = open(fd_path, O_RDWR);
	if (fd < 0) {
		GUEST_CHANNEL_LOG(ERR, "Unable to connect to '%s' with error "
				"%s", fd_path, strerror(errno));
		return -1;
	}

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		GUEST_CHANNEL_LOG(ERR, "Failed on fcntl get flags for file %s",
				fd_path);
		goto error;
	}

	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0) {
		GUEST_CHANNEL_LOG(ERR, "Failed on setting non-blocking mode for "
				"file %s", fd_path);
		goto error;
	}
	/* QEMU needs a delay after connection */
	sleep(1);

	/* Send a test packet, this command is ignored by the host, but a successful
	 * send indicates that the host endpoint is monitoring.
	 */
	pkt.command = RTE_POWER_CPU_POWER_CONNECT;
	global_fds[lcore_id] = fd;
	ret = guest_channel_send_msg(&pkt, lcore_id);
	if (ret != 0) {
		GUEST_CHANNEL_LOG(ERR,
				"Error on channel '%s' communications test: %s",
				fd_path, ret > 0 ? strerror(ret) :
				"channel not connected");
		goto error;
	}
	GUEST_CHANNEL_LOG(INFO, "Channel '%s' is now connected", fd_path);
	return 0;
error:
	close(fd);
	global_fds[lcore_id] = -1;
	return -1;
}

int
guest_channel_send_msg(struct rte_power_channel_packet *pkt,
		unsigned int lcore_id)
{
	int ret, buffer_len = sizeof(*pkt);
	void *buffer = pkt;

	if (lcore_id >= RTE_MAX_LCORE) {
		GUEST_CHANNEL_LOG(ERR, "Channel(%u) is out of range 0...%d",
				lcore_id, RTE_MAX_LCORE-1);
		return -1;
	}

	if (global_fds[lcore_id] < 0) {
		GUEST_CHANNEL_LOG(ERR, "Channel is not connected");
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

int rte_power_guest_channel_send_msg(struct rte_power_channel_packet *pkt,
			unsigned int lcore_id)
{
	return guest_channel_send_msg(pkt, lcore_id);
}

int power_guest_channel_read_msg(void *pkt,
		size_t pkt_len,
		unsigned int lcore_id)
{
	int ret;
	struct pollfd fds;

	if (pkt_len == 0 || pkt == NULL)
		return -1;

	if (lcore_id >= RTE_MAX_LCORE) {
		GUEST_CHANNEL_LOG(ERR, "Channel(%u) is out of range 0...%d",
				lcore_id, RTE_MAX_LCORE-1);
		return -1;
	}

	if (global_fds[lcore_id] < 0) {
		GUEST_CHANNEL_LOG(ERR, "Channel is not connected");
		return -1;
	}

	fds.fd = global_fds[lcore_id];
	fds.events = POLLIN;

	ret = poll(&fds, 1, TIMEOUT);
	if (ret == 0) {
		GUEST_CHANNEL_LOG(DEBUG, "Timeout occurred during poll function.");
		return -1;
	} else if (ret < 0) {
		GUEST_CHANNEL_LOG(ERR, "Error occurred during poll function: %s",
				strerror(errno));
		return -1;
	}

	while (pkt_len > 0) {
		ret = read(global_fds[lcore_id],
				pkt, pkt_len);

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}

		if (ret == 0) {
			GUEST_CHANNEL_LOG(ERR, "Expected more data, but connection has been closed.");
			return -1;
		}
		pkt = (char *)pkt + ret;
		pkt_len -= ret;
	}

	return 0;
}

int rte_power_guest_channel_receive_msg(void *pkt,
		size_t pkt_len,
		unsigned int lcore_id)
{
	return power_guest_channel_read_msg(pkt, pkt_len, lcore_id);
}

void
guest_channel_host_disconnect(unsigned int lcore_id)
{
	if (lcore_id >= RTE_MAX_LCORE) {
		GUEST_CHANNEL_LOG(ERR, "Channel(%u) is out of range 0...%d",
				lcore_id, RTE_MAX_LCORE-1);
		return;
	}
	if (global_fds[lcore_id] < 0)
		return;
	close(global_fds[lcore_id]);
	global_fds[lcore_id] = -1;
}
