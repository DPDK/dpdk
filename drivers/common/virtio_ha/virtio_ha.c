/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <sys/uio.h>
#include <sys/socket.h>
#include <unistd.h>

#include <rte_log.h>

#include "virtio_ha.h"

RTE_LOG_REGISTER(virtio_ha_ipc_logtype, pmd.vdpa.virtio, NOTICE);
#define HA_IPC_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, virtio_ha_ipc_logtype, \
		"VIRTIO HA IPC %s(): " fmt "\n", __func__, ##args)

struct virtio_ha_msg *
virtio_ha_alloc_msg(void)
{
	struct virtio_ha_msg *msg;
	int i;

	msg = malloc(sizeof(struct virtio_ha_msg));
	if (msg == NULL) {
		HA_IPC_LOG(ERR, "Failed to alloc HA message");
		return NULL;
	}

	memset(msg, 0, sizeof(struct virtio_ha_msg));

	for (i = 0; i < VIRTIO_HA_MAX_FDS; i++)
		msg->fds[i] = -1;

	return msg;
}

inline void
virtio_ha_free_msg(struct virtio_ha_msg *msg)
{
	/* Assume fds will be used later, so don't close fd here */
	free(msg);
}

void
virtio_ha_reset_msg(struct virtio_ha_msg *msg)
{
	int i;

	memset(msg, 0, sizeof(struct virtio_ha_msg));

	for (i = 0; i < VIRTIO_HA_MAX_FDS; i++)
		msg->fds[i] = -1;	
}

static void
close_msg_fds(struct virtio_ha_msg *msg)
{
	int i;

	for (i = 0; i < msg->nr_fds; i++) {
		int fd = msg->fds[i];

		if (fd == -1)
			continue;

		msg->fds[i] = -1;
		close(fd);
	}
}

/*
 * return bytes# of read on success or negative val on failure. Update fdnum
 * with number of fds read.
 */
static int
read_fd_message(int sockfd, char *buf, int buflen, int *fds, int max_fds, int *fd_num)
{
	struct iovec iov;
	struct msghdr msgh;
	char control[CMSG_SPACE(max_fds * sizeof(int))];
	struct cmsghdr *cmsg;
	int got_fds = 0;
	int ret;

	*fd_num = 0;

	memset(&msgh, 0, sizeof(msgh));
	iov.iov_base = buf;
	iov.iov_len  = buflen;

	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;
	msgh.msg_control = control;
	msgh.msg_controllen = sizeof(control);

	ret = recvmsg(sockfd, &msgh, 0);
	if (ret <= 0) {
		if (ret)
			HA_IPC_LOG(ERR, "Failed to recvmsg on fd %d (%s)",
				sockfd, strerror(errno));
		return ret;
	}

	if (msgh.msg_flags & MSG_TRUNC)
		HA_IPC_LOG(ERR, "Truncated message (fd %d)", sockfd);

	/* MSG_CTRUNC may be caused by LSM misconfiguration */
	if (msgh.msg_flags & MSG_CTRUNC)
		HA_IPC_LOG(ERR, "Truncated control data (fd %d)", sockfd);

	for (cmsg = CMSG_FIRSTHDR(&msgh); cmsg != NULL;
		cmsg = CMSG_NXTHDR(&msgh, cmsg)) {
		if ((cmsg->cmsg_level == SOL_SOCKET) &&
			(cmsg->cmsg_type == SCM_RIGHTS)) {
			got_fds = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
			*fd_num = got_fds;
			memcpy(fds, CMSG_DATA(cmsg), got_fds * sizeof(int));
			break;
		}
	}

	/* Clear out unused file descriptors */
	while (got_fds < max_fds)
		fds[got_fds++] = -1;

	return ret;
}

static int
send_fd_message(int sockfd, struct iovec *iov, size_t nr_iov, int *fds, int fd_num, size_t msg_sz)
{
	struct msghdr msgh;
	size_t fdsize = fd_num * sizeof(int);
	char control[CMSG_SPACE(fdsize)];
	struct cmsghdr *cmsg;
	int ret;

	memset(&msgh, 0, sizeof(msgh));

	msgh.msg_iov = iov;
	msgh.msg_iovlen = nr_iov;

	if (fds && fd_num > 0) {
		msgh.msg_control = control;
		msgh.msg_controllen = sizeof(control);
		cmsg = CMSG_FIRSTHDR(&msgh);
		if (cmsg == NULL) {
			HA_IPC_LOG(ERR, "Missing cmsg");
			errno = EINVAL;
			return -1;
		}
		cmsg->cmsg_len = CMSG_LEN(fdsize);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		memcpy(CMSG_DATA(cmsg), fds, fdsize);
	} else {
		msgh.msg_control = NULL;
		msgh.msg_controllen = 0;
	}

	do {
		ret = sendmsg(sockfd, &msgh, MSG_NOSIGNAL);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to sendmsg on fd %d (%s)",
			sockfd, strerror(errno));
		return ret;
	} else if (ret != (int)msg_sz) {
		HA_IPC_LOG(ERR, "Failed to send complete msg on fd %d", sockfd);
		return -1;		
	}

	return ret;
}

int
virtio_ha_send_msg(int sockfd, struct virtio_ha_msg *msg)
{
	struct virtio_ha_msg_hdr *hdr = &msg->hdr;
	struct iovec iov[2];
	size_t nr_iov = 1, msg_sz = 0;

	iov[0].iov_base = hdr;
	iov[0].iov_len = sizeof(*hdr);
	msg_sz += sizeof(*hdr);

	if (hdr->size) {
		iov[1].iov_base = msg->iov.iov_base;
		iov[1].iov_len = msg->iov.iov_len;
		msg_sz += msg->iov.iov_len;
		nr_iov++;
	}

	return send_fd_message(sockfd, iov, nr_iov, msg->fds, msg->nr_fds, msg_sz);
}

int
virtio_ha_recv_msg(int sockfd, struct virtio_ha_msg *msg)
{
	struct virtio_ha_msg_hdr *hdr = &msg->hdr;
	int ret;

	ret = read_fd_message(sockfd, (char *)hdr, sizeof(*hdr),
		msg->fds, VIRTIO_HA_MAX_FDS, &msg->nr_fds);
	if (ret <= 0)
		goto out;

	if (ret != sizeof(*hdr)) {
		HA_IPC_LOG(ERR, "Unexpected header size read\n");
		ret = -1;
		goto out;
	}

	if (hdr->size) {
		msg->iov.iov_len = hdr->size;
		msg->iov.iov_base = malloc(msg->iov.iov_len);
		if (msg->iov.iov_base == NULL) {
			HA_IPC_LOG(ERR, "Failed to alloc message payload when read message");
			ret = -1;
			goto out;
		}
		ret = read(sockfd, msg->iov.iov_base, msg->iov.iov_len);
		if (ret <= 0)
			goto out;
		if (ret != (int)msg->iov.iov_len) {
			HA_IPC_LOG(ERR, "Failed to read complete message payload (fd %d)", sockfd);
			ret = -1;
			goto out;
		}
	}

out:
	if (ret <= 0)
		close_msg_fds(msg);

	return ret;
}
