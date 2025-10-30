/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#include <errno.h>
#include <inttypes.h>
#include <linux/netlink.h>
#include <net/if.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>

#include <rte_malloc.h>
#include <tap_netlink.h>
#include <rte_random.h>

#include "tap_log.h"

/* Compatibility with glibc < 2.24 */
#ifndef SOL_NETLINK
#define SOL_NETLINK     270
#endif

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
#ifdef NETLINK_EXT_ACK
	int one = 1;
#endif

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

#ifdef NETLINK_EXT_ACK
	/* Ask for extended ACK response. on older kernel will ignore request. */
	if (setsockopt(fd, SOL_NETLINK, NETLINK_EXT_ACK, &one, sizeof(one)) < 0)
		TAP_LOG(NOTICE, "Unable to request netlink error information");
#endif

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
	int send_bytes;

	nh->nlmsg_pid = 0; /* communication with the kernel uses pid 0 */
	nh->nlmsg_seq = (uint32_t)rte_rand();

retry:
	send_bytes = send(nlsk_fd, nh, nh->nlmsg_len, 0);
	if (send_bytes < 0) {
		if (errno == EINTR)
			goto retry;

		TAP_LOG(ERR, "Failed to send netlink message: %s (%d)",
			strerror(errno), errno);
		return -1;
	}
	return send_bytes;
}

#ifdef NETLINK_EXT_ACK
static const struct nlattr *
tap_nl_attr_first(const struct nlmsghdr *nh, size_t offset)
{
	return (const struct nlattr *)((const char *)nh + NLMSG_SPACE(offset));
}

static const struct nlattr *
tap_nl_attr_next(const struct nlattr *attr)
{
	return (const struct nlattr *)((const char *)attr
				       + NLMSG_ALIGN(attr->nla_len));
}

static bool
tap_nl_attr_ok(const struct nlattr *attr, int len)
{
	if (len < (int)sizeof(struct nlattr))
		return false; /* missing header */
	if (attr->nla_len < sizeof(struct nlattr))
		return false; /* attribute length should include itself */
	if ((int)attr->nla_len  > len)
		return false; /* attribute is truncated */
	return true;
}


/* Decode extended errors from kernel */
static void
tap_nl_dump_ext_ack(const struct nlmsghdr *nh, const struct nlmsgerr *err)
{
	const struct nlattr *attr;
	const char *tail = (const char *)nh + NLMSG_ALIGN(nh->nlmsg_len);
	size_t hlen = sizeof(*err);

	/* no TLVs, no extended response */
	if (!(nh->nlmsg_flags & NLM_F_ACK_TLVS))
		return;

	if (!(nh->nlmsg_flags & NLM_F_CAPPED))
		hlen += err->msg.nlmsg_len - NLMSG_HDRLEN;

	for (attr = tap_nl_attr_first(nh, hlen);
	     tap_nl_attr_ok(attr, tail - (const char *)attr);
	     attr = tap_nl_attr_next(attr)) {
		uint16_t type = attr->nla_type & NLA_TYPE_MASK;

		if (type == NLMSGERR_ATTR_MSG) {
			const char *msg = (const char *)attr
				+ NLMSG_ALIGN(sizeof(*attr));

			if (err->error)
				TAP_LOG(ERR, "%s", msg);
			else

				TAP_LOG(WARNING, "%s", msg);
			break;
		}
	}
}
#else
/*
 * External ACK support was added in Linux kernel 4.17
 * on older kernels, just ignore that part of message
 */
#define tap_nl_dump_ext_ack(nh, err) do { } while (0)
#endif

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
	char buf[BUF_SIZE];
	int multipart = 0;
	int ret = 0;

	do {
		struct nlmsghdr *nh;
		int recv_bytes;

retry:
		recv_bytes = recv(nlsk_fd, buf, sizeof(buf), 0);
		if (recv_bytes < 0) {
			if (errno == EINTR)
				goto retry;
			return -1;
		}

		for (nh = (struct nlmsghdr *)buf;
		     NLMSG_OK(nh, (unsigned int)recv_bytes);
		     nh = NLMSG_NEXT(nh, recv_bytes)) {
			if (nh->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *err_data = NLMSG_DATA(nh);

				tap_nl_dump_ext_ack(nh, err_data);
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
tap_nlattr_add(struct tap_nlmsg *msg, unsigned short type,
	   unsigned int data_len, const void *data)
{
	/* see man 3 rtnetlink */
	struct rtattr *rta;

	rta = (struct rtattr *)NLMSG_TAIL(msg);
	rta->rta_len = RTA_LENGTH(data_len);
	rta->rta_type = type;
	if (data_len > 0)
		memcpy(RTA_DATA(rta), data, data_len);
	msg->nh.nlmsg_len = NLMSG_ALIGN(msg->nh.nlmsg_len) + RTA_ALIGN(rta->rta_len);
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
tap_nlattr_add8(struct tap_nlmsg *msg, unsigned short type, uint8_t data)
{
	tap_nlattr_add(msg, type, sizeof(uint8_t), &data);
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
tap_nlattr_add16(struct tap_nlmsg *msg, unsigned short type, uint16_t data)
{
	tap_nlattr_add(msg, type, sizeof(uint16_t), &data);
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
tap_nlattr_add32(struct tap_nlmsg *msg, unsigned short type, uint32_t data)
{
	tap_nlattr_add(msg, type, sizeof(uint32_t), &data);
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
tap_nlattr_nested_start(struct tap_nlmsg *msg, uint16_t type)
{
	struct nested_tail *tail;

	tail = rte_zmalloc(NULL, sizeof(struct nested_tail), 0);
	if (!tail) {
		TAP_LOG(ERR,
			"Couldn't allocate memory for nested netlink attribute");
		return -1;
	}

	tail->tail = (struct rtattr *)NLMSG_TAIL(msg);

	tap_nlattr_add(msg, type, 0, NULL);

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
tap_nlattr_nested_finish(struct tap_nlmsg *msg)
{
	struct nested_tail *tail = msg->nested_tails;

	tail->tail->rta_len = (char *)NLMSG_TAIL(msg) - (char *)tail->tail;

	if (tail->prev)
		msg->nested_tails = tail->prev;

	rte_free(tail);
}

/**
 * Helper structure to pass data between netlink request and callback
 */
struct link_info_ctx {
	struct ifinfomsg *info;
	struct rte_ether_addr *mac;
	unsigned int *flags;
	unsigned int ifindex;
	int found;
};

/**
 * Callback to extract link information from RTM_GETLINK response
 */
static int
tap_nl_link_cb(struct nlmsghdr *nh, void *arg)
{
	struct link_info_ctx *ctx = arg;
	struct ifinfomsg *ifi = NLMSG_DATA(nh);
	struct rtattr *rta;
	int rta_len;

	if (nh->nlmsg_type != RTM_NEWLINK)
		return 0;

	/* Check if this is the interface we're looking for */
	if (ifi->ifi_index != (int)ctx->ifindex)
		return 0;

	ctx->found = 1;

	/* Copy basic info if requested */
	if (ctx->info)
		*ctx->info = *ifi;

	/* Extract flags if requested */
	if (ctx->flags)
		*ctx->flags = ifi->ifi_flags;

	/* Parse attributes for MAC address if requested */
	if (ctx->mac) {
		rta = IFLA_RTA(ifi);
		rta_len = IFLA_PAYLOAD(nh);

		for (; RTA_OK(rta, rta_len); rta = RTA_NEXT(rta, rta_len)) {
			if (rta->rta_type == IFLA_ADDRESS) {
				if (RTA_PAYLOAD(rta) >= RTE_ETHER_ADDR_LEN)
					memcpy(ctx->mac, RTA_DATA(rta),
					       RTE_ETHER_ADDR_LEN);
				break;
			}
		}
	}

	return 0;
}

/**
 * Get interface flags by ifindex
 *
 * @param nlsk_fd
 *   Netlink socket file descriptor
 * @param ifindex
 *   Interface index
 * @param flags
 *   Pointer to store interface flags
 *
 * @return
 *   0 on success, -1 on error
 */
int
tap_nl_get_flags(int nlsk_fd, unsigned int ifindex, unsigned int *flags)
{
	struct {
		struct nlmsghdr nh;
		struct ifinfomsg ifi;
	} req = {
		.nh = {
			.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
			.nlmsg_type = RTM_GETLINK,
			.nlmsg_flags = NLM_F_REQUEST,
		},
		.ifi = {
			.ifi_family = AF_UNSPEC,
			.ifi_index = ifindex,
		},
	};
	struct link_info_ctx ctx = {
		.flags = flags,
		.ifindex = ifindex,
		.found = 0,
	};

	if (tap_nl_send(nlsk_fd, &req.nh) < 0)
		return -1;

	if (tap_nl_recv(nlsk_fd, tap_nl_link_cb, &ctx) < 0)
		return -1;

	if (!ctx.found) {
		errno = ENODEV;
		return -1;
	}

	return 0;
}

/**
 * Set interface flags by ifindex
 *
 * @param nlsk_fd
 *   Netlink socket file descriptor
 * @param ifindex
 *   Interface index
 * @param flags
 *   Flags to set/unset
 * @param set
 *   1 to set flags, 0 to unset them
 *
 * @return
 *   0 on success, -1 on error
 */
int
tap_nl_set_flags(int nlsk_fd, unsigned int ifindex, unsigned int flags, int set)
{
	struct {
		struct nlmsghdr nh;
		struct ifinfomsg ifi;
	} req = {
		.nh = {
			.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
			.nlmsg_type = RTM_SETLINK,
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK,
		},
		.ifi = {
			.ifi_family = AF_UNSPEC,
			.ifi_index = ifindex,
			.ifi_flags = set ? flags : 0,
			.ifi_change = flags,  /* mask of flags to change */
		},
	};

	if (tap_nl_send(nlsk_fd, &req.nh) < 0)
		return -1;

	return tap_nl_recv_ack(nlsk_fd);
}

/**
 * Set interface MTU by ifindex
 *
 * @param nlsk_fd
 *   Netlink socket file descriptor
 * @param ifindex
 *   Interface index
 * @param mtu
 *   New MTU value
 *
 * @return
 *   0 on success, -1 on error
 */
int
tap_nl_set_mtu(int nlsk_fd, unsigned int ifindex, unsigned int mtu)
{
	struct {
		struct nlmsghdr nh;
		struct ifinfomsg ifi;
		char buf[64];
	} req = {
		.nh = {
			.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
			.nlmsg_type = RTM_SETLINK,
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK,
		},
		.ifi = {
			.ifi_family = AF_UNSPEC,
			.ifi_index = ifindex,
		},
	};
	struct rtattr *rta;

	/* Add MTU attribute */
	rta = (struct rtattr *)((char *)&req + NLMSG_ALIGN(req.nh.nlmsg_len));
	rta->rta_type = IFLA_MTU;
	rta->rta_len = RTA_LENGTH(sizeof(mtu));
	memcpy(RTA_DATA(rta), &mtu, sizeof(mtu));
	req.nh.nlmsg_len = NLMSG_ALIGN(req.nh.nlmsg_len) + RTA_ALIGN(rta->rta_len);

	if (tap_nl_send(nlsk_fd, &req.nh) < 0)
		return -1;

	return tap_nl_recv_ack(nlsk_fd);
}

/**
 * Get interface MAC address by ifindex
 *
 * @param nlsk_fd
 *   Netlink socket file descriptor
 * @param ifindex
 *   Interface index
 * @param mac
 *   Pointer to store MAC address
 *
 * @return
 *   0 on success, -1 on error
 */
int
tap_nl_get_mac(int nlsk_fd, unsigned int ifindex, struct rte_ether_addr *mac)
{
	struct {
		struct nlmsghdr nh;
		struct ifinfomsg ifi;
	} req = {
		.nh = {
			.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
			.nlmsg_type = RTM_GETLINK,
			.nlmsg_flags = NLM_F_REQUEST,
		},
		.ifi = {
			.ifi_family = AF_UNSPEC,
			.ifi_index = ifindex,
		},
	};
	struct link_info_ctx ctx = {
		.mac = mac,
		.ifindex = ifindex,
		.found = 0,
	};

	if (tap_nl_send(nlsk_fd, &req.nh) < 0)
		return -1;

	if (tap_nl_recv(nlsk_fd, tap_nl_link_cb, &ctx) < 0)
		return -1;

	if (!ctx.found) {
		errno = ENODEV;
		return -1;
	}

	return 0;
}

/**
 * Set interface MAC address by ifindex
 *
 * @param nlsk_fd
 *   Netlink socket file descriptor
 * @param ifindex
 *   Interface index
 * @param mac
 *   New MAC address
 *
 * @return
 *   0 on success, -1 on error
 */
int
tap_nl_set_mac(int nlsk_fd, unsigned int ifindex, const struct rte_ether_addr *mac)
{
	struct {
		struct nlmsghdr nh;
		struct ifinfomsg ifi;
		char buf[64];
	} req = {
		.nh = {
			.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
			.nlmsg_type = RTM_SETLINK,
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK,
		},
		.ifi = {
			.ifi_family = AF_UNSPEC,
			.ifi_index = ifindex,
		},
	};
	struct rtattr *rta;

	/* Add MAC address attribute */
	rta = (struct rtattr *)((char *)&req + NLMSG_ALIGN(req.nh.nlmsg_len));
	rta->rta_type = IFLA_ADDRESS;
	rta->rta_len = RTA_LENGTH(RTE_ETHER_ADDR_LEN);
	memcpy(RTA_DATA(rta), mac, RTE_ETHER_ADDR_LEN);
	req.nh.nlmsg_len = NLMSG_ALIGN(req.nh.nlmsg_len) + RTA_ALIGN(rta->rta_len);

	if (tap_nl_send(nlsk_fd, &req.nh) < 0)
		return -1;

	return tap_nl_recv_ack(nlsk_fd);
}
