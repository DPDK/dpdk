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
#include <fcntl.h>
#include <sys/socket.h>
#include <pthread.h>

/* sys/un.h with __USE_MISC uses strlen, which is unsafe */
#ifdef __USE_MISC
#define REMOVED_USE_MISC
#undef __USE_MISC
#endif
#include <sys/un.h>
/* make sure we redefine __USE_MISC only if it was previously undefined */
#ifdef REMOVED_USE_MISC
#define __USE_MISC
#undef REMOVED_USE_MISC
#endif

#include <rte_log.h>
#include <rte_pci.h>
#include <rte_eal_memconfig.h>
#include <rte_malloc.h>

#include "eal_filesystem.h"
#include "eal_pci_init.h"
#include "eal_thread.h"

/**
 * @file
 * VFIO socket for communication between primary and secondary processes.
 *
 * This file is only compiled if CONFIG_RTE_EAL_VFIO is set to "y".
 */

#ifdef VFIO_PRESENT

#define SOCKET_PATH_FMT "%s/.%s_mp_socket"
#define CMSGLEN (CMSG_LEN(sizeof(int)))
#define FD_TO_CMSGHDR(fd, chdr) \
		do {\
			(chdr).cmsg_len = CMSGLEN;\
			(chdr).cmsg_level = SOL_SOCKET;\
			(chdr).cmsg_type = SCM_RIGHTS;\
			memcpy((chdr).__cmsg_data, &(fd), sizeof(fd));\
		} while (0)
#define CMSGHDR_TO_FD(chdr, fd) \
			memcpy(&(fd), (chdr).__cmsg_data, sizeof(fd))

static pthread_t socket_thread;
static int mp_socket_fd;


/* get socket path (/var/run if root, $HOME otherwise) */
static void
get_socket_path(char *buffer, int bufsz)
{
	const char *dir = "/var/run";
	const char *home_dir = getenv("HOME");

	if (getuid() != 0 && home_dir != NULL)
		dir = home_dir;

	/* use current prefix as file path */
	snprintf(buffer, bufsz, SOCKET_PATH_FMT, dir,
			internal_config.hugefile_prefix);
}



/*
 * data flow for socket comm protocol:
 * 1. client sends SOCKET_REQ_CONTAINER or SOCKET_REQ_GROUP
 * 1a. in case of SOCKET_REQ_GROUP, client also then sends group number
 * 2. server receives message
 * 2a. in case of invalid group, SOCKET_ERR is sent back to client
 * 2b. in case of unbound group, SOCKET_NO_FD is sent back to client
 * 2c. in case of valid group, SOCKET_OK is sent and immediately followed by fd
 *
 * in case of any error, socket is closed.
 */

/* send a request, return -1 on error */
int
vfio_mp_sync_send_request(int socket, int req)
{
	struct msghdr hdr;
	struct iovec iov;
	int buf;
	int ret;

	memset(&hdr, 0, sizeof(hdr));

	buf = req;

	hdr.msg_iov = &iov;
	hdr.msg_iovlen = 1;
	iov.iov_base = (char *) &buf;
	iov.iov_len = sizeof(buf);

	ret = sendmsg(socket, &hdr, 0);
	if (ret < 0)
		return -1;
	return 0;
}

/* receive a request and return it */
int
vfio_mp_sync_receive_request(int socket)
{
	int buf;
	struct msghdr hdr;
	struct iovec iov;
	int ret, req;

	memset(&hdr, 0, sizeof(hdr));

	buf = SOCKET_ERR;

	hdr.msg_iov = &iov;
	hdr.msg_iovlen = 1;
	iov.iov_base = (char *) &buf;
	iov.iov_len = sizeof(buf);

	ret = recvmsg(socket, &hdr, 0);
	if (ret < 0)
		return -1;

	req = buf;

	return req;
}

/* send OK in message, fd in control message */
int
vfio_mp_sync_send_fd(int socket, int fd)
{
	int buf;
	struct msghdr hdr;
	struct cmsghdr *chdr;
	char chdr_buf[CMSGLEN];
	struct iovec iov;
	int ret;

	chdr = (struct cmsghdr *) chdr_buf;
	memset(chdr, 0, sizeof(chdr_buf));
	memset(&hdr, 0, sizeof(hdr));

	hdr.msg_iov = &iov;
	hdr.msg_iovlen = 1;
	iov.iov_base = (char *) &buf;
	iov.iov_len = sizeof(buf);
	hdr.msg_control = chdr;
	hdr.msg_controllen = CMSGLEN;

	buf = SOCKET_OK;
	FD_TO_CMSGHDR(fd, *chdr);

	ret = sendmsg(socket, &hdr, 0);
	if (ret < 0)
		return -1;
	return 0;
}

/* receive OK in message, fd in control message */
int
vfio_mp_sync_receive_fd(int socket)
{
	int buf;
	struct msghdr hdr;
	struct cmsghdr *chdr;
	char chdr_buf[CMSGLEN];
	struct iovec iov;
	int ret, req, fd;

	buf = SOCKET_ERR;

	chdr = (struct cmsghdr *) chdr_buf;
	memset(chdr, 0, sizeof(chdr_buf));
	memset(&hdr, 0, sizeof(hdr));

	hdr.msg_iov = &iov;
	hdr.msg_iovlen = 1;
	iov.iov_base = (char *) &buf;
	iov.iov_len = sizeof(buf);
	hdr.msg_control = chdr;
	hdr.msg_controllen = CMSGLEN;

	ret = recvmsg(socket, &hdr, 0);
	if (ret < 0)
		return -1;

	req = buf;

	if (req != SOCKET_OK)
		return -1;

	CMSGHDR_TO_FD(*chdr, fd);

	return fd;
}

/* connect socket_fd in secondary process to the primary process's socket */
int
vfio_mp_sync_connect_to_primary(void)
{
	struct sockaddr_un addr;
	socklen_t sockaddr_len;
	int socket_fd;

	/* set up a socket */
	socket_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (socket_fd < 0) {
		RTE_LOG(ERR, EAL, "Failed to create socket!\n");
		return -1;
	}

	get_socket_path(addr.sun_path, sizeof(addr.sun_path));
	addr.sun_family = AF_UNIX;

	sockaddr_len = sizeof(struct sockaddr_un);

	if (connect(socket_fd, (struct sockaddr *) &addr, sockaddr_len) == 0)
		return socket_fd;

	/* if connect failed */
	close(socket_fd);
	return -1;
}



/*
 * socket listening thread for primary process
 */
static __attribute__((noreturn)) void *
vfio_mp_sync_thread(void __rte_unused * arg)
{
	int ret, fd, vfio_group_no;

	/* wait for requests on the socket */
	for (;;) {
		int conn_sock;
		struct sockaddr_un addr;
		socklen_t sockaddr_len = sizeof(addr);

		/* this is a blocking call */
		conn_sock = accept(mp_socket_fd, (struct sockaddr *) &addr,
				&sockaddr_len);

		/* just restart on error */
		if (conn_sock == -1)
			continue;

		/* set socket to linger after close */
		struct linger l;
		l.l_onoff = 1;
		l.l_linger = 60;

		if (setsockopt(conn_sock, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) < 0)
			RTE_LOG(WARNING, EAL, "Cannot set SO_LINGER option "
					"on listen socket (%s)\n", strerror(errno));

		ret = vfio_mp_sync_receive_request(conn_sock);

		switch (ret) {
		case SOCKET_REQ_CONTAINER:
			fd = vfio_get_container_fd();
			if (fd < 0)
				vfio_mp_sync_send_request(conn_sock, SOCKET_ERR);
			else
				vfio_mp_sync_send_fd(conn_sock, fd);
			break;
		case SOCKET_REQ_GROUP:
			/* wait for group number */
			vfio_group_no = vfio_mp_sync_receive_request(conn_sock);
			if (vfio_group_no < 0) {
				close(conn_sock);
				continue;
			}

			fd = vfio_get_group_fd(vfio_group_no);

			if (fd < 0)
				vfio_mp_sync_send_request(conn_sock, SOCKET_ERR);
			/* if VFIO group exists but isn't bound to VFIO driver */
			else if (fd == 0)
				vfio_mp_sync_send_request(conn_sock, SOCKET_NO_FD);
			/* if group exists and is bound to VFIO driver */
			else {
				vfio_mp_sync_send_request(conn_sock, SOCKET_OK);
				vfio_mp_sync_send_fd(conn_sock, fd);
			}
			break;
		default:
			vfio_mp_sync_send_request(conn_sock, SOCKET_ERR);
			break;
		}
		close(conn_sock);
	}
}

static int
vfio_mp_sync_socket_setup(void)
{
	int ret, socket_fd;
	struct sockaddr_un addr;
	socklen_t sockaddr_len;

	/* set up a socket */
	socket_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (socket_fd < 0) {
		RTE_LOG(ERR, EAL, "Failed to create socket!\n");
		return -1;
	}

	get_socket_path(addr.sun_path, sizeof(addr.sun_path));
	addr.sun_family = AF_UNIX;

	sockaddr_len = sizeof(struct sockaddr_un);

	unlink(addr.sun_path);

	ret = bind(socket_fd, (struct sockaddr *) &addr, sockaddr_len);
	if (ret) {
		RTE_LOG(ERR, EAL, "Failed to bind socket: %s!\n", strerror(errno));
		close(socket_fd);
		return -1;
	}

	ret = listen(socket_fd, 50);
	if (ret) {
		RTE_LOG(ERR, EAL, "Failed to listen: %s!\n", strerror(errno));
		close(socket_fd);
		return -1;
	}

	/* save the socket in local configuration */
	mp_socket_fd = socket_fd;

	return 0;
}

/*
 * set up a local socket and tell it to listen for incoming connections
 */
int
vfio_mp_sync_setup(void)
{
	int ret;
	char thread_name[RTE_MAX_THREAD_NAME_LEN];

	if (vfio_mp_sync_socket_setup() < 0) {
		RTE_LOG(ERR, EAL, "Failed to set up local socket!\n");
		return -1;
	}

	ret = pthread_create(&socket_thread, NULL,
			vfio_mp_sync_thread, NULL);
	if (ret) {
		RTE_LOG(ERR, EAL,
			"Failed to create thread for communication with secondary processes!\n");
		close(mp_socket_fd);
		return -1;
	}

	/* Set thread_name for aid in debugging. */
	snprintf(thread_name, RTE_MAX_THREAD_NAME_LEN, "vfio-sync");
	ret = rte_thread_setname(socket_thread, thread_name);
	if (ret)
		RTE_LOG(DEBUG, EAL,
			"Failed to set thread name for secondary processes!\n");

	return 0;
}

#endif
