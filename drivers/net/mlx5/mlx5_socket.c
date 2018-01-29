/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "mlx5.h"
#include "mlx5_utils.h"

/**
 * Initialise the socket to communicate with the secondary process
 *
 * @param[in] priv
 *   Pointer to private structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
priv_socket_init(struct priv *priv)
{
	struct sockaddr_un sun = {
		.sun_family = AF_UNIX,
	};
	int ret;
	int flags;
	struct stat file_stat;

	/*
	 * Initialise the socket to communicate with the secondary
	 * process.
	 */
	ret = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ret < 0) {
		WARN("secondary process not supported: %s", strerror(errno));
		return ret;
	}
	priv->primary_socket = ret;
	flags = fcntl(priv->primary_socket, F_GETFL, 0);
	if (flags == -1)
		goto out;
	ret = fcntl(priv->primary_socket, F_SETFL, flags | O_NONBLOCK);
	if (ret < 0)
		goto out;
	snprintf(sun.sun_path, sizeof(sun.sun_path), "/var/tmp/%s_%d",
		 MLX5_DRIVER_NAME, priv->primary_socket);
	ret = stat(sun.sun_path, &file_stat);
	if (!ret)
		claim_zero(remove(sun.sun_path));
	ret = bind(priv->primary_socket, (const struct sockaddr *)&sun,
		   sizeof(sun));
	if (ret < 0) {
		WARN("cannot bind socket, secondary process not supported: %s",
		     strerror(errno));
		goto close;
	}
	ret = listen(priv->primary_socket, 0);
	if (ret < 0) {
		WARN("Secondary process not supported: %s", strerror(errno));
		goto close;
	}
	return ret;
close:
	remove(sun.sun_path);
out:
	claim_zero(close(priv->primary_socket));
	priv->primary_socket = 0;
	return -(ret);
}

/**
 * Un-Initialise the socket to communicate with the secondary process
 *
 * @param[in] priv
 *   Pointer to private structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
priv_socket_uninit(struct priv *priv)
{
	MKSTR(path, "/var/tmp/%s_%d", MLX5_DRIVER_NAME, priv->primary_socket);
	claim_zero(close(priv->primary_socket));
	priv->primary_socket = 0;
	claim_zero(remove(path));
	return 0;
}

/**
 * Handle socket interrupts.
 *
 * @param priv
 *   Pointer to private structure.
 */
void
priv_socket_handle(struct priv *priv)
{
	int conn_sock;
	int ret = 0;
	struct cmsghdr *cmsg = NULL;
	struct ucred *cred = NULL;
	char buf[CMSG_SPACE(sizeof(struct ucred))] = { 0 };
	char vbuf[1024] = { 0 };
	struct iovec io = {
		.iov_base = vbuf,
		.iov_len = sizeof(*vbuf),
	};
	struct msghdr msg = {
		.msg_iov = &io,
		.msg_iovlen = 1,
		.msg_control = buf,
		.msg_controllen = sizeof(buf),
	};
	int *fd;

	/* Accept the connection from the client. */
	conn_sock = accept(priv->primary_socket, NULL, NULL);
	if (conn_sock < 0) {
		WARN("connection failed: %s", strerror(errno));
		return;
	}
	ret = setsockopt(conn_sock, SOL_SOCKET, SO_PASSCRED, &(int){1},
					 sizeof(int));
	if (ret < 0) {
		WARN("cannot change socket options");
		goto out;
	}
	ret = recvmsg(conn_sock, &msg, MSG_WAITALL);
	if (ret < 0) {
		WARN("received an empty message: %s", strerror(errno));
		goto out;
	}
	/* Expect to receive credentials only. */
	cmsg = CMSG_FIRSTHDR(&msg);
	if (cmsg == NULL) {
		WARN("no message");
		goto out;
	}
	if ((cmsg->cmsg_type == SCM_CREDENTIALS) &&
		(cmsg->cmsg_len >= sizeof(*cred))) {
		cred = (struct ucred *)CMSG_DATA(cmsg);
		assert(cred != NULL);
	}
	cmsg = CMSG_NXTHDR(&msg, cmsg);
	if (cmsg != NULL) {
		WARN("Message wrongly formatted");
		goto out;
	}
	/* Make sure all the ancillary data was received and valid. */
	if ((cred == NULL) || (cred->uid != getuid()) ||
	    (cred->gid != getgid())) {
		WARN("wrong credentials");
		goto out;
	}
	/* Set-up the ancillary data. */
	cmsg = CMSG_FIRSTHDR(&msg);
	assert(cmsg != NULL);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(priv->ctx->cmd_fd));
	fd = (int *)CMSG_DATA(cmsg);
	*fd = priv->ctx->cmd_fd;
	ret = sendmsg(conn_sock, &msg, 0);
	if (ret < 0)
		WARN("cannot send response");
out:
	close(conn_sock);
}

/**
 * Connect to the primary process.
 *
 * @param[in] priv
 *   Pointer to private structure.
 *
 * @return
 *   fd on success, negative errno value on failure.
 */
int
priv_socket_connect(struct priv *priv)
{
	struct sockaddr_un sun = {
		.sun_family = AF_UNIX,
	};
	int socket_fd;
	int *fd = NULL;
	int ret;
	struct ucred *cred;
	char buf[CMSG_SPACE(sizeof(*cred))] = { 0 };
	char vbuf[1024] = { 0 };
	struct iovec io = {
		.iov_base = vbuf,
		.iov_len = sizeof(*vbuf),
	};
	struct msghdr msg = {
		.msg_control = buf,
		.msg_controllen = sizeof(buf),
		.msg_iov = &io,
		.msg_iovlen = 1,
	};
	struct cmsghdr *cmsg;

	ret = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ret < 0) {
		WARN("cannot connect to primary");
		return ret;
	}
	socket_fd = ret;
	snprintf(sun.sun_path, sizeof(sun.sun_path), "/var/tmp/%s_%d",
		 MLX5_DRIVER_NAME, priv->primary_socket);
	ret = connect(socket_fd, (const struct sockaddr *)&sun, sizeof(sun));
	if (ret < 0) {
		WARN("cannot connect to primary");
		goto out;
	}
	cmsg = CMSG_FIRSTHDR(&msg);
	if (cmsg == NULL) {
		DEBUG("cannot get first message");
		goto out;
	}
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_CREDENTIALS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(*cred));
	cred = (struct ucred *)CMSG_DATA(cmsg);
	if (cred == NULL) {
		DEBUG("no credentials received");
		goto out;
	}
	cred->pid = getpid();
	cred->uid = getuid();
	cred->gid = getgid();
	ret = sendmsg(socket_fd, &msg, MSG_DONTWAIT);
	if (ret < 0) {
		WARN("cannot send credentials to primary: %s",
		     strerror(errno));
		goto out;
	}
	ret = recvmsg(socket_fd, &msg, MSG_WAITALL);
	if (ret <= 0) {
		WARN("no message from primary: %s", strerror(errno));
		goto out;
	}
	cmsg = CMSG_FIRSTHDR(&msg);
	if (cmsg == NULL) {
		WARN("No file descriptor received");
		goto out;
	}
	fd = (int *)CMSG_DATA(cmsg);
	if (*fd <= 0) {
		WARN("no file descriptor received: %s", strerror(errno));
		ret = *fd;
		goto out;
	}
	ret = *fd;
out:
	close(socket_fd);
	return ret;
}
