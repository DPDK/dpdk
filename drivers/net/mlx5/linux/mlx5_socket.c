/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "rte_eal.h"
#include "mlx5_utils.h"
#include "mlx5.h"

/* PMD socket service for tools. */

#define MLX5_SOCKET_PATH "/var/tmp/dpdk_net_mlx5_%d"

int server_socket; /* Unix socket for primary process. */
struct rte_intr_handle server_intr_handle; /* Interrupt handler. */

/**
 * Handle server pmd socket interrupts.
 */
static void
mlx5_pmd_socket_handle(void *cb __rte_unused)
{
	int conn_sock;
	int ret;
	struct cmsghdr *cmsg = NULL;
	int data;
	char buf[CMSG_SPACE(sizeof(int))] = { 0 };
	struct iovec io = {
		.iov_base = &data,
		.iov_len = sizeof(data),
	};
	struct msghdr msg = {
		.msg_iov = &io,
		.msg_iovlen = 1,
		.msg_control = buf,
		.msg_controllen = sizeof(buf),
	};
	uint16_t port_id;
	int fd;
	FILE *file = NULL;
	struct rte_eth_dev *dev;

	/* Accept the connection from the client. */
	conn_sock = accept(server_socket, NULL, NULL);
	if (conn_sock < 0) {
		DRV_LOG(WARNING, "connection failed: %s", strerror(errno));
		return;
	}
	ret = recvmsg(conn_sock, &msg, MSG_WAITALL);
	if (ret < 0) {
		DRV_LOG(WARNING, "wrong message received: %s",
			strerror(errno));
		goto error;
	}
	/* Receive file descriptor. */
	cmsg = CMSG_FIRSTHDR(&msg);
	if (cmsg == NULL || cmsg->cmsg_type != SCM_RIGHTS ||
	    cmsg->cmsg_len < sizeof(int)) {
		DRV_LOG(WARNING, "invalid file descriptor message");
		goto error;
	}
	memcpy(&fd, CMSG_DATA(cmsg), sizeof(fd));
	file = fdopen(fd, "w");
	if (!file) {
		DRV_LOG(WARNING, "Failed to open file");
		goto error;
	}
	/* Receive port number. */
	if (msg.msg_iovlen != 1 || msg.msg_iov->iov_len < sizeof(uint16_t)) {
		DRV_LOG(WARNING, "wrong port number message");
		goto error;
	}
	memcpy(&port_id, msg.msg_iov->iov_base, sizeof(port_id));
	if (!rte_eth_dev_is_valid_port(port_id)) {
		DRV_LOG(WARNING, "Invalid port %u", port_id);
		goto error;
	}
	/* Dump flow. */
	dev = &rte_eth_devices[port_id];
	ret = mlx5_flow_dev_dump(dev, file, NULL);
	/* Set-up the ancillary data and reply. */
	msg.msg_controllen = 0;
	msg.msg_control = NULL;
	msg.msg_iovlen = 1;
	msg.msg_iov = &io;
	data = -ret;
	io.iov_len = sizeof(data);
	io.iov_base = &data;
	do {
		ret = sendmsg(conn_sock, &msg, 0);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0)
		DRV_LOG(WARNING, "failed to send response %s",
			strerror(errno));
error:
	if (conn_sock >= 0)
		close(conn_sock);
	if (file)
		fclose(file);
}

/**
 * Install interrupt handler.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @return
 *   0 on success, a negative errno value otherwise.
 */
static int
mlx5_pmd_interrupt_handler_install(void)
{
	MLX5_ASSERT(server_socket);
	server_intr_handle.fd = server_socket;
	server_intr_handle.type = RTE_INTR_HANDLE_EXT;
	return rte_intr_callback_register(&server_intr_handle,
					  mlx5_pmd_socket_handle, NULL);
}

/**
 * Uninstall interrupt handler.
 */
static void
mlx5_pmd_interrupt_handler_uninstall(void)
{
	if (server_socket) {
		mlx5_intr_callback_unregister(&server_intr_handle,
					      mlx5_pmd_socket_handle,
					      NULL);
	}
	server_intr_handle.fd = 0;
	server_intr_handle.type = RTE_INTR_HANDLE_UNKNOWN;
}

/**
 * Initialise the socket to communicate with the secondary process
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int
mlx5_pmd_socket_init(void)
{
	struct sockaddr_un sun = {
		.sun_family = AF_UNIX,
	};
	int ret;
	int flags;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	if (server_socket)
		return 0;
	/*
	 * Initialize the socket to communicate with the secondary
	 * process.
	 */
	ret = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ret < 0) {
		DRV_LOG(WARNING, "Failed to open mlx5 socket: %s",
			strerror(errno));
		goto error;
	}
	server_socket = ret;
	flags = fcntl(server_socket, F_GETFL, 0);
	if (flags == -1)
		goto error;
	ret = fcntl(server_socket, F_SETFL, flags | O_NONBLOCK);
	if (ret < 0)
		goto error;
	snprintf(sun.sun_path, sizeof(sun.sun_path), MLX5_SOCKET_PATH,
		 getpid());
	remove(sun.sun_path);
	ret = bind(server_socket, (const struct sockaddr *)&sun, sizeof(sun));
	if (ret < 0) {
		DRV_LOG(WARNING,
			"cannot bind mlx5 socket: %s", strerror(errno));
		goto close;
	}
	ret = listen(server_socket, 0);
	if (ret < 0) {
		DRV_LOG(WARNING, "cannot listen on mlx5 socket: %s",
			strerror(errno));
		goto close;
	}
	if (mlx5_pmd_interrupt_handler_install()) {
		DRV_LOG(WARNING, "cannot register interrupt handler for mlx5 socket: %s",
			strerror(errno));
		goto close;
	}
	return 0;
close:
	remove(sun.sun_path);
error:
	claim_zero(close(server_socket));
	server_socket = 0;
	DRV_LOG(ERR, "Cannot initialize socket: %s", strerror(errno));
	return -errno;
}

/**
 * Un-Initialize the pmd socket
 */
RTE_FINI(mlx5_pmd_socket_uninit)
{
	if (!server_socket)
		return;
	mlx5_pmd_interrupt_handler_uninstall();
	claim_zero(close(server_socket));
	server_socket = 0;
	MKSTR(path, MLX5_SOCKET_PATH, getpid());
	claim_zero(remove(path));
}
