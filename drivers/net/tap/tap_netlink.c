/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#include <errno.h>
#include <inttypes.h>
#include <linux/netlink.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <rte_malloc.h>
#include <tap_netlink.h>
#include <rte_random.h>
#include "tap_log.h"

/* Must be quite large to support dumping a huge list of QDISC or filters. */
#define BUF_SIZE (32 * 1024) /* Size of the buffer to receive kernel messages */
#define SNDBUF_SIZE 32768 /* Send buffer size for the netlink socket */
#define RCVBUF_SIZE 32768 /* Receive buffer size for the netlink socket */

struct nested_tail {
	struct rtattr *tail;
	struct nested_tail *prev;
};

/**
 * Initialize a netlink socket for communicating with the kernel.
 *
 * @param nl_groups
 *   Set it to a netlink group value (e.g. RTMGRP_LINK) to receive messages for
 *   specific netlink multicast groups. Otherwise, no subscription will be made.
 *
 * @return
 *   netlink socket file descriptor on success, -1 otherwise.
 */
int
tap_nl_init(uint32_t nl_groups)
{
	int fd, sndbuf_size = SNDBUF_SIZE, rcvbuf_size = RCVBUF_SIZE;
	struct sockaddr_nl local = {
		.nl_family = AF_NETLINK,
		.nl_groups = nl_groups,
	};

	fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
	if (fd < 0) {
		TAP_LOG(ERR, "Unable to create a netlink socket");
		return -1;
	}
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf_size, sizeof(int))) {
		TAP_LOG(ERR, "Unable to set socket buffer send size");
		close(fd);
		return -1;
	}
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, sizeof(int))) {
		TAP_LOG(ERR, "Unable to set socket buffer receive size");
		close(fd);
		return -1;
	}
	if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
		TAP_LOG(ERR, "Unable to bind to the netlink socket");
		close(fd);
		return -1;
	}
	return fd;
}

/**
 * Clean up a netlink socket once all communicating with the kernel is finished.
 *
 * @param[in] nlsk_fd
 *   The netlink socket file descriptor used for communication.
 *
 * @return
 *   0 on success, -1 otherwise.
 */
int
tap_nl_final(int nlsk_fd)
{
	if (close(nlsk_fd)) {
		TAP_LOG(ERR, "Failed to close netlink socket: %s (%d)",
			strerror(errno), errno);
		return -1;
	}
	return 0;
}

/**
 * Send a message to the kernel on the netlink socket.
 *
 * @param[in] nlsk_fd
 *   The netlink socket file descriptor used for communication.
 * @param[in] nh
 *   The netlink message send to the kernel.
 *
 * @return
 *   the number of sent bytes on success, -1 otherwise.
 */
int
tap_nl_send(int nlsk_fd, struct nlmsghdr *nh)
{
	/* man 7 netlink EXAMPLE */
	struct sockaddr_nl sa = {
		.nl_family = AF_NETLINK,
	};
	struct iovec iov = {
		.iov_base = nh,
		.iov_len = nh->nlmsg_len,
	};
	struct msghdr msg = {
		.msg_name = &sa,
		.msg_namelen = sizeof(sa),
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};
	int send_bytes;

	nh->nlmsg_pid = 0; /* communication with the kernel uses pid 0 */
	nh->nlmsg_seq = (uint32_t)rte_rand();
	send_bytes = sendmsg(nlsk_fd, &msg, 0);
	if (send_bytes < 0) {
		TAP_LOG(ERR, "Failed to send netlink message: %s (%d)",
			strerror(errno), errno);
		return -1;
	}
	return send_bytes;
}

/**
 * Check that the kernel sends an appropriate ACK in response
 * to an tap_nl_send().
 *
 * @param[in] nlsk_fd
 *   The netlink socket file descriptor used for communication.
 *
 * @return
 *   0 on success, -1 otherwise with errno set.
 */
int
tap_nl_recv_ack(int nlsk_fd)
{
	return tap_nl_recv(nlsk_fd, NULL, NULL);
}

/**
 * Receive a message from the kernel on the netlink socket, following an
 * tap_nl_send().
 *
 * @param[in] nlsk_fd
 *   The netlink socket file descriptor used for communication.
 * @param[in] cb
 *   The callback function to call for each netlink message received.
 * @param[in, out] arg
 *   Custom arguments for the callback.
 *
 * @return
 *   0 on success, -1 otherwise with errno set.
 */
int
tap_nl_recv(int nlsk_fd, int (*cb)(struct nlmsghdr *, void *arg), void *arg)
{
	/* man 7 netlink EXAMPLE */
	struct sockaddr_nl sa;
	char buf[BUF_SIZE];
	struct iovec iov = {
		.iov_base = buf,
		.iov_len = sizeof(buf),
	};
	struct msghdr msg = {
		.msg_name = &sa,
		.msg_namelen = sizeof(sa),
		.msg_iov = &iov,
		/* One message at a time */
		.msg_iovlen = 1,
	};
	int multipart = 0;
	int ret = 0;

	do {
		struct nlmsghdr *nh;
		int recv_bytes = 0;

		recv_bytes = recvmsg(nlsk_fd, &msg, 0);
		if (recv_bytes < 0)
			return -1;
		for (nh = (struct nlmsghdr *)buf;
		     NLMSG_OK(nh, (unsigned int)recv_bytes);
		     nh = NLMSG_NEXT(nh, recv_bytes)) {
			if (nh->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *err_data = NLMSG_DATA(nh);

				if (err_data->error < 0) {
					errno = -err_data->error;
					return -1;
				}
				/* Ack message. */
				return 0;
			}
			/* Multi-part msgs and their trailing DONE message. */
			if (nh->nlmsg_flags & NLM_F_MULTI) {
				if (nh->nlmsg_type == NLMSG_DONE)
					return 0;
				multipart = 1;
			}
			if (cb)
				ret = cb(nh, arg);
		}
	} while (multipart);
	return ret;
}

/**
 * Append a netlink attribute to a message.
 *
 * @param[in, out] nh
 *   The netlink message to parse, received from the kernel.
 * @param[in] type
 *   The type of attribute to append.
 * @param[in] data_len
 *   The length of the data to append.
 * @param[in] data
 *   The data to append.
 */
void
tap_nlattr_add(struct nlmsghdr *nh, unsigned short type,
	   unsigned int data_len, const void *data)
{
	/* see man 3 rtnetlink */
	struct rtattr *rta;

	rta = (struct rtattr *)NLMSG_TAIL(nh);
	rta->rta_len = RTA_LENGTH(data_len);
	rta->rta_type = type;
	memcpy(RTA_DATA(rta), data, data_len);
	nh->nlmsg_len = NLMSG_ALIGN(nh->nlmsg_len) + RTA_ALIGN(rta->rta_len);
}

/**
 * Append a uint8_t netlink attribute to a message.
 *
 * @param[in, out] nh
 *   The netlink message to parse, received from the kernel.
 * @param[in] type
 *   The type of attribute to append.
 * @param[in] data
 *   The data to append.
 */
void
tap_nlattr_add8(struct nlmsghdr *nh, unsigned short type, uint8_t data)
{
	tap_nlattr_add(nh, type, sizeof(uint8_t), &data);
}

/**
 * Append a uint16_t netlink attribute to a message.
 *
 * @param[in, out] nh
 *   The netlink message to parse, received from the kernel.
 * @param[in] type
 *   The type of attribute to append.
 * @param[in] data
 *   The data to append.
 */
void
tap_nlattr_add16(struct nlmsghdr *nh, unsigned short type, uint16_t data)
{
	tap_nlattr_add(nh, type, sizeof(uint16_t), &data);
}

/**
 * Append a uint16_t netlink attribute to a message.
 *
 * @param[in, out] nh
 *   The netlink message to parse, received from the kernel.
 * @param[in] type
 *   The type of attribute to append.
 * @param[in] data
 *   The data to append.
 */
void
tap_nlattr_add32(struct nlmsghdr *nh, unsigned short type, uint32_t data)
{
	tap_nlattr_add(nh, type, sizeof(uint32_t), &data);
}

/**
 * Start a nested netlink attribute.
 * It must be followed later by a call to tap_nlattr_nested_finish().
 *
 * @param[in, out] msg
 *   The netlink message where to edit the nested_tails metadata.
 * @param[in] type
 *   The nested attribute type to append.
 *
 * @return
 *   -1 if adding a nested netlink attribute failed, 0 otherwise.
 */
int
tap_nlattr_nested_start(struct nlmsg *msg, uint16_t type)
{
	struct nested_tail *tail;

	tail = rte_zmalloc(NULL, sizeof(struct nested_tail), 0);
	if (!tail) {
		TAP_LOG(ERR,
			"Couldn't allocate memory for nested netlink attribute");
		return -1;
	}

	tail->tail = (struct rtattr *)NLMSG_TAIL(&msg->nh);

	tap_nlattr_add(&msg->nh, type, 0, NULL);

	tail->prev = msg->nested_tails;

	msg->nested_tails = tail;

	return 0;
}

/**
 * End a nested netlink attribute.
 * It follows a call to tap_nlattr_nested_start().
 * In effect, it will modify the nested attribute length to include every bytes
 * from the nested attribute start, up to here.
 *
 * @param[in, out] msg
 *   The netlink message where to edit the nested_tails metadata.
 */
void
tap_nlattr_nested_finish(struct nlmsg *msg)
{
	struct nested_tail *tail = msg->nested_tails;

	tail->tail->rta_len = (char *)NLMSG_TAIL(&msg->nh) - (char *)tail->tail;

	if (tail->prev)
		msg->nested_tails = tail->prev;

	rte_free(tail);
}
