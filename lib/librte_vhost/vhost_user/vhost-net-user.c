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
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <pthread.h>

#include <rte_log.h>
#include <rte_virtio_net.h>

#include "fd_man.h"
#include "vhost-net-user.h"
#include "vhost-net.h"
#include "virtio-net-user.h"

/*
 * Every time rte_vhost_driver_register() is invoked, an associated
 * vhost_user_socket struct will be created.
 */
struct vhost_user_socket {
	char *path;
	int listenfd;
};

struct vhost_user_connection {
	struct vhost_user_socket *vsocket;
	int vid;
};

#define MAX_VHOST_SOCKET 1024
struct vhost_user {
	struct vhost_user_socket *vsockets[MAX_VHOST_SOCKET];
	struct fdset fdset;
	int vsocket_cnt;
	pthread_mutex_t mutex;
};

#define MAX_VIRTIO_BACKLOG 128

static void vhost_user_new_connection(int fd, void *data, int *remove);
static void vhost_user_msg_handler(int fd, void *dat, int *remove);

static struct vhost_user vhost_user = {
	.fdset = {
		.fd = { [0 ... MAX_FDS - 1] = {-1, NULL, NULL, NULL, 0} },
		.fd_mutex = PTHREAD_MUTEX_INITIALIZER,
		.num = 0
	},
	.vsocket_cnt = 0,
	.mutex = PTHREAD_MUTEX_INITIALIZER,
};

static const char *vhost_message_str[VHOST_USER_MAX] = {
	[VHOST_USER_NONE] = "VHOST_USER_NONE",
	[VHOST_USER_GET_FEATURES] = "VHOST_USER_GET_FEATURES",
	[VHOST_USER_SET_FEATURES] = "VHOST_USER_SET_FEATURES",
	[VHOST_USER_SET_OWNER] = "VHOST_USER_SET_OWNER",
	[VHOST_USER_RESET_OWNER] = "VHOST_USER_RESET_OWNER",
	[VHOST_USER_SET_MEM_TABLE] = "VHOST_USER_SET_MEM_TABLE",
	[VHOST_USER_SET_LOG_BASE] = "VHOST_USER_SET_LOG_BASE",
	[VHOST_USER_SET_LOG_FD] = "VHOST_USER_SET_LOG_FD",
	[VHOST_USER_SET_VRING_NUM] = "VHOST_USER_SET_VRING_NUM",
	[VHOST_USER_SET_VRING_ADDR] = "VHOST_USER_SET_VRING_ADDR",
	[VHOST_USER_SET_VRING_BASE] = "VHOST_USER_SET_VRING_BASE",
	[VHOST_USER_GET_VRING_BASE] = "VHOST_USER_GET_VRING_BASE",
	[VHOST_USER_SET_VRING_KICK] = "VHOST_USER_SET_VRING_KICK",
	[VHOST_USER_SET_VRING_CALL] = "VHOST_USER_SET_VRING_CALL",
	[VHOST_USER_SET_VRING_ERR]  = "VHOST_USER_SET_VRING_ERR",
	[VHOST_USER_GET_PROTOCOL_FEATURES]  = "VHOST_USER_GET_PROTOCOL_FEATURES",
	[VHOST_USER_SET_PROTOCOL_FEATURES]  = "VHOST_USER_SET_PROTOCOL_FEATURES",
	[VHOST_USER_GET_QUEUE_NUM]  = "VHOST_USER_GET_QUEUE_NUM",
	[VHOST_USER_SET_VRING_ENABLE]  = "VHOST_USER_SET_VRING_ENABLE",
	[VHOST_USER_SEND_RARP]  = "VHOST_USER_SEND_RARP",
};

/**
 * Create a unix domain socket, bind to path and listen for connection.
 * @return
 *  socket fd or -1 on failure
 */
static int
uds_socket(const char *path)
{
	struct sockaddr_un un;
	int sockfd;
	int ret;

	if (path == NULL)
		return -1;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0)
		return -1;
	RTE_LOG(INFO, VHOST_CONFIG, "socket created, fd:%d\n", sockfd);

	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	snprintf(un.sun_path, sizeof(un.sun_path), "%s", path);
	ret = bind(sockfd, (struct sockaddr *)&un, sizeof(un));
	if (ret == -1) {
		RTE_LOG(ERR, VHOST_CONFIG, "fail to bind fd:%d, remove file:%s and try again.\n",
			sockfd, path);
		goto err;
	}
	RTE_LOG(INFO, VHOST_CONFIG, "bind to %s\n", path);

	ret = listen(sockfd, MAX_VIRTIO_BACKLOG);
	if (ret == -1)
		goto err;

	return sockfd;

err:
	close(sockfd);
	return -1;
}

/* return bytes# of read on success or negative val on failure. */
static int
read_fd_message(int sockfd, char *buf, int buflen, int *fds, int fd_num)
{
	struct iovec iov;
	struct msghdr msgh;
	size_t fdsize = fd_num * sizeof(int);
	char control[CMSG_SPACE(fdsize)];
	struct cmsghdr *cmsg;
	int ret;

	memset(&msgh, 0, sizeof(msgh));
	iov.iov_base = buf;
	iov.iov_len  = buflen;

	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;
	msgh.msg_control = control;
	msgh.msg_controllen = sizeof(control);

	ret = recvmsg(sockfd, &msgh, 0);
	if (ret <= 0) {
		RTE_LOG(ERR, VHOST_CONFIG, "recvmsg failed\n");
		return ret;
	}

	if (msgh.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) {
		RTE_LOG(ERR, VHOST_CONFIG, "truncted msg\n");
		return -1;
	}

	for (cmsg = CMSG_FIRSTHDR(&msgh); cmsg != NULL;
		cmsg = CMSG_NXTHDR(&msgh, cmsg)) {
		if ((cmsg->cmsg_level == SOL_SOCKET) &&
			(cmsg->cmsg_type == SCM_RIGHTS)) {
			memcpy(fds, CMSG_DATA(cmsg), fdsize);
			break;
		}
	}

	return ret;
}

/* return bytes# of read on success or negative val on failure. */
static int
read_vhost_message(int sockfd, struct VhostUserMsg *msg)
{
	int ret;

	ret = read_fd_message(sockfd, (char *)msg, VHOST_USER_HDR_SIZE,
		msg->fds, VHOST_MEMORY_MAX_NREGIONS);
	if (ret <= 0)
		return ret;

	if (msg && msg->size) {
		if (msg->size > sizeof(msg->payload)) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"invalid msg size: %d\n", msg->size);
			return -1;
		}
		ret = read(sockfd, &msg->payload, msg->size);
		if (ret <= 0)
			return ret;
		if (ret != (int)msg->size) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"read control message failed\n");
			return -1;
		}
	}

	return ret;
}

static int
send_fd_message(int sockfd, char *buf, int buflen, int *fds, int fd_num)
{

	struct iovec iov;
	struct msghdr msgh;
	size_t fdsize = fd_num * sizeof(int);
	char control[CMSG_SPACE(fdsize)];
	struct cmsghdr *cmsg;
	int ret;

	memset(&msgh, 0, sizeof(msgh));
	iov.iov_base = buf;
	iov.iov_len = buflen;

	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;

	if (fds && fd_num > 0) {
		msgh.msg_control = control;
		msgh.msg_controllen = sizeof(control);
		cmsg = CMSG_FIRSTHDR(&msgh);
		cmsg->cmsg_len = CMSG_LEN(fdsize);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		memcpy(CMSG_DATA(cmsg), fds, fdsize);
	} else {
		msgh.msg_control = NULL;
		msgh.msg_controllen = 0;
	}

	do {
		ret = sendmsg(sockfd, &msgh, 0);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,  "sendmsg error\n");
		return ret;
	}

	return ret;
}

static int
send_vhost_message(int sockfd, struct VhostUserMsg *msg)
{
	int ret;

	if (!msg)
		return 0;

	msg->flags &= ~VHOST_USER_VERSION_MASK;
	msg->flags |= VHOST_USER_VERSION;
	msg->flags |= VHOST_USER_REPLY_MASK;

	ret = send_fd_message(sockfd, (char *)msg,
		VHOST_USER_HDR_SIZE + msg->size, NULL, 0);

	return ret;
}

/* call back when there is new vhost-user connection.  */
static void
vhost_user_new_connection(int fd, void *dat, int *remove __rte_unused)
{
	struct vhost_user_socket *vsocket = dat;
	int conn_fd;
	struct vhost_user_connection *conn;
	int vid;
	unsigned int size;

	conn_fd = accept(fd, NULL, NULL);
	RTE_LOG(INFO, VHOST_CONFIG,
		"new virtio connection is %d\n", conn_fd);
	if (conn_fd < 0)
		return;

	conn = calloc(1, sizeof(*conn));
	if (conn == NULL) {
		close(conn_fd);
		return;
	}

	vid = vhost_new_device();
	if (vid == -1) {
		free(conn);
		close(conn_fd);
		return;
	}

	size = strnlen(vsocket->path, PATH_MAX);
	vhost_set_ifname(vid, vsocket->path, size);

	RTE_LOG(INFO, VHOST_CONFIG, "new device, handle is %d\n", vid);

	conn->vsocket = vsocket;
	conn->vid = vid;
	fdset_add(&vhost_user.fdset,
		conn_fd, vhost_user_msg_handler, NULL, conn);
}

/* callback when there is message on the connfd */
static void
vhost_user_msg_handler(int connfd, void *dat, int *remove)
{
	int vid;
	struct vhost_user_connection *conn = dat;
	struct VhostUserMsg msg;
	uint64_t features;
	int ret;

	vid = conn->vid;
	ret = read_vhost_message(connfd, &msg);
	if (ret <= 0 || msg.request >= VHOST_USER_MAX) {
		if (ret < 0)
			RTE_LOG(ERR, VHOST_CONFIG,
				"vhost read message failed\n");
		else if (ret == 0)
			RTE_LOG(INFO, VHOST_CONFIG,
				"vhost peer closed\n");
		else
			RTE_LOG(ERR, VHOST_CONFIG,
				"vhost read incorrect message\n");

		close(connfd);
		*remove = 1;
		free(conn);
		vhost_destroy_device(vid);

		return;
	}

	RTE_LOG(INFO, VHOST_CONFIG, "read message %s\n",
		vhost_message_str[msg.request]);
	switch (msg.request) {
	case VHOST_USER_GET_FEATURES:
		ret = vhost_get_features(vid, &features);
		msg.payload.u64 = features;
		msg.size = sizeof(msg.payload.u64);
		send_vhost_message(connfd, &msg);
		break;
	case VHOST_USER_SET_FEATURES:
		features = msg.payload.u64;
		vhost_set_features(vid, &features);
		break;

	case VHOST_USER_GET_PROTOCOL_FEATURES:
		msg.payload.u64 = VHOST_USER_PROTOCOL_FEATURES;
		msg.size = sizeof(msg.payload.u64);
		send_vhost_message(connfd, &msg);
		break;
	case VHOST_USER_SET_PROTOCOL_FEATURES:
		user_set_protocol_features(vid, msg.payload.u64);
		break;

	case VHOST_USER_SET_OWNER:
		vhost_set_owner(vid);
		break;
	case VHOST_USER_RESET_OWNER:
		vhost_reset_owner(vid);
		break;

	case VHOST_USER_SET_MEM_TABLE:
		user_set_mem_table(vid, &msg);
		break;

	case VHOST_USER_SET_LOG_BASE:
		user_set_log_base(vid, &msg);

		/* it needs a reply */
		msg.size = sizeof(msg.payload.u64);
		send_vhost_message(connfd, &msg);
		break;
	case VHOST_USER_SET_LOG_FD:
		close(msg.fds[0]);
		RTE_LOG(INFO, VHOST_CONFIG, "not implemented.\n");
		break;

	case VHOST_USER_SET_VRING_NUM:
		vhost_set_vring_num(vid, &msg.payload.state);
		break;
	case VHOST_USER_SET_VRING_ADDR:
		vhost_set_vring_addr(vid, &msg.payload.addr);
		break;
	case VHOST_USER_SET_VRING_BASE:
		vhost_set_vring_base(vid, &msg.payload.state);
		break;

	case VHOST_USER_GET_VRING_BASE:
		ret = user_get_vring_base(vid, &msg.payload.state);
		msg.size = sizeof(msg.payload.state);
		send_vhost_message(connfd, &msg);
		break;

	case VHOST_USER_SET_VRING_KICK:
		user_set_vring_kick(vid, &msg);
		break;
	case VHOST_USER_SET_VRING_CALL:
		user_set_vring_call(vid, &msg);
		break;

	case VHOST_USER_SET_VRING_ERR:
		if (!(msg.payload.u64 & VHOST_USER_VRING_NOFD_MASK))
			close(msg.fds[0]);
		RTE_LOG(INFO, VHOST_CONFIG, "not implemented\n");
		break;

	case VHOST_USER_GET_QUEUE_NUM:
		msg.payload.u64 = VHOST_MAX_QUEUE_PAIRS;
		msg.size = sizeof(msg.payload.u64);
		send_vhost_message(connfd, &msg);
		break;

	case VHOST_USER_SET_VRING_ENABLE:
		user_set_vring_enable(vid, &msg.payload.state);
		break;
	case VHOST_USER_SEND_RARP:
		user_send_rarp(vid, &msg);
		break;

	default:
		break;

	}
}

/**
 * Creates and initialise the vhost server.
 */
int
rte_vhost_driver_register(const char *path)
{
	struct vhost_user_socket *vsocket;

	pthread_mutex_lock(&vhost_user.mutex);

	if (vhost_user.vsocket_cnt == MAX_VHOST_SOCKET) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"error: the number of servers reaches maximum\n");
		pthread_mutex_unlock(&vhost_user.mutex);
		return -1;
	}

	vsocket = calloc(sizeof(struct vhost_user_socket), 1);
	if (vsocket == NULL) {
		pthread_mutex_unlock(&vhost_user.mutex);
		return -1;
	}

	vsocket->listenfd = uds_socket(path);
	if (vsocket->listenfd < 0) {
		free(vsocket);
		pthread_mutex_unlock(&vhost_user.mutex);
		return -1;
	}

	vsocket->path = strdup(path);

	fdset_add(&vhost_user.fdset, vsocket->listenfd,
		vhost_user_new_connection, NULL, vsocket);

	vhost_user.vsockets[vhost_user.vsocket_cnt++] = vsocket;
	pthread_mutex_unlock(&vhost_user.mutex);

	return 0;
}


/**
 * Unregister the specified vhost server
 */
int
rte_vhost_driver_unregister(const char *path)
{
	int i;
	int count;

	pthread_mutex_lock(&vhost_user.mutex);

	for (i = 0; i < vhost_user.vsocket_cnt; i++) {
		if (!strcmp(vhost_user.vsockets[i]->path, path)) {
			fdset_del(&vhost_user.fdset,
				vhost_user.vsockets[i]->listenfd);

			close(vhost_user.vsockets[i]->listenfd);
			free(vhost_user.vsockets[i]->path);
			free(vhost_user.vsockets[i]);

			unlink(path);

			count = --vhost_user.vsocket_cnt;
			vhost_user.vsockets[i] = vhost_user.vsockets[count];
			vhost_user.vsockets[count] = NULL;
			pthread_mutex_unlock(&vhost_user.mutex);

			return 0;
		}
	}
	pthread_mutex_unlock(&vhost_user.mutex);

	return -1;
}

int
rte_vhost_driver_session_start(void)
{
	fdset_event_dispatch(&vhost_user.fdset);
	return 0;
}
