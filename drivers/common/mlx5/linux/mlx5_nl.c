/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 6WIND S.A.
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <errno.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>
#include <linux/genetlink.h>
#include <net/if.h>
#include <rdma/rdma_netlink.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdalign.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <eal_export.h>
#include <rte_errno.h>

#include "mlx5_nl.h"
#include "../mlx5_common_log.h"
#include "mlx5_malloc.h"
#ifdef HAVE_DEVLINK
#include <linux/devlink.h>
#endif


/* Size of the buffer to receive kernel messages */
#define MLX5_NL_BUF_SIZE (32 * 1024)
/* Send buffer size for the Netlink socket */
#define MLX5_SEND_BUF_SIZE 32768
/* Receive buffer size for the Netlink socket */
#define MLX5_RECV_BUF_SIZE 32768
/* Maximal physical port name length. */
#define MLX5_PHYS_PORT_NAME_MAX 128

/** Parameters of VLAN devices created by driver. */
#define MLX5_VMWA_VLAN_DEVICE_PFX "evmlx"
/*
 * Define NDA_RTA as defined in iproute2 sources.
 *
 * see in iproute2 sources file include/libnetlink.h
 */
#ifndef MLX5_NDA_RTA
#define MLX5_NDA_RTA(r) \
	((struct rtattr *)(((char *)(r)) + NLMSG_ALIGN(sizeof(struct ndmsg))))
#endif
/*
 * Define NLMSG_TAIL as defined in iproute2 sources.
 *
 * see in iproute2 sources file include/libnetlink.h
 */
#ifndef NLMSG_TAIL
#define NLMSG_TAIL(nmsg) \
	((struct rtattr *)(((char *)(nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))
#endif
/*
 * The following definitions are normally found in rdma/rdma_netlink.h,
 * however they are so recent that most systems do not expose them yet.
 */
#ifndef HAVE_RDMA_NL_NLDEV
#define RDMA_NL_NLDEV 5
#endif
#ifndef HAVE_RDMA_NLDEV_CMD_GET
#define RDMA_NLDEV_CMD_GET 1
#endif
#ifndef HAVE_RDMA_NLDEV_CMD_PORT_GET
#define RDMA_NLDEV_CMD_PORT_GET 5
#endif
#ifndef HAVE_RDMA_NLDEV_ATTR_DEV_INDEX
#define RDMA_NLDEV_ATTR_DEV_INDEX 1
#endif
#ifndef HAVE_RDMA_NLDEV_ATTR_DEV_NAME
#define RDMA_NLDEV_ATTR_DEV_NAME 2
#endif
#ifndef HAVE_RDMA_NLDEV_ATTR_PORT_INDEX
#define RDMA_NLDEV_ATTR_PORT_INDEX 3
#endif
#ifndef HAVE_RDMA_NLDEV_ATTR_PORT_STATE
#define RDMA_NLDEV_ATTR_PORT_STATE 12
#endif
#ifndef HAVE_RDMA_NLDEV_ATTR_NDEV_INDEX
#define RDMA_NLDEV_ATTR_NDEV_INDEX 50
#endif
#ifndef HAVE_RDMA_NLDEV_ATTR_EVENT_TYPE
#define RDMA_NLDEV_ATTR_EVENT_TYPE 102
#define RDMA_NETDEV_ATTACH_EVENT 2
#define RDMA_NETDEV_DETACH_EVENT 3
#endif
#ifndef HAVE_RDMA_NLDEV_SYS_ATTR_MONITOR_MODE
#define RDMA_NLDEV_SYS_ATTR_MONITOR_MODE 103
#endif
#ifndef HAVE_RDMA_NLDEV_CMD_MONITOR
#define RDMA_NLDEV_CMD_MONITOR 28
#endif
#ifndef HAVE_RDMA_NLDEV_CMD_SYS_GET
#define RDMA_NLDEV_CMD_SYS_GET 6
#endif
#ifndef HAVE_RDMA_NL_GROUP_NOTIFY
#define RDMA_NL_GROUP_NOTIFY 4
#endif
#define RDMA_NL_GROUP_NOTIFICATION (1 << (RDMA_NL_GROUP_NOTIFY - 1))

/* These are normally found in linux/if_link.h. */
#ifndef HAVE_IFLA_NUM_VF
#define IFLA_NUM_VF 21
#endif
#ifndef HAVE_IFLA_EXT_MASK
#define IFLA_EXT_MASK 29
#endif
#ifndef HAVE_IFLA_PHYS_SWITCH_ID
#define IFLA_PHYS_SWITCH_ID 36
#endif
#ifndef HAVE_IFLA_PHYS_PORT_NAME
#define IFLA_PHYS_PORT_NAME 38
#endif

/*
 * Some Devlink defines may be missed in old kernel versions,
 * adjust used defines.
 */
#ifndef DEVLINK_GENL_NAME
#define DEVLINK_GENL_NAME "devlink"
#endif
#ifndef DEVLINK_GENL_VERSION
#define DEVLINK_GENL_VERSION 1
#endif
#ifndef DEVLINK_ATTR_BUS_NAME
#define DEVLINK_ATTR_BUS_NAME 1
#endif
#ifndef DEVLINK_ATTR_DEV_NAME
#define DEVLINK_ATTR_DEV_NAME 2
#endif
#ifndef DEVLINK_ATTR_PARAM
#define DEVLINK_ATTR_PARAM 80
#endif
#ifndef DEVLINK_ATTR_PARAM_NAME
#define DEVLINK_ATTR_PARAM_NAME 81
#endif
#ifndef DEVLINK_ATTR_PARAM_TYPE
#define DEVLINK_ATTR_PARAM_TYPE 83
#endif
#ifndef DEVLINK_ATTR_PARAM_VALUES_LIST
#define DEVLINK_ATTR_PARAM_VALUES_LIST 84
#endif
#ifndef DEVLINK_ATTR_PARAM_VALUE
#define DEVLINK_ATTR_PARAM_VALUE 85
#endif
#ifndef DEVLINK_ATTR_PARAM_VALUE_DATA
#define DEVLINK_ATTR_PARAM_VALUE_DATA 86
#endif
#ifndef DEVLINK_ATTR_PARAM_VALUE_CMODE
#define DEVLINK_ATTR_PARAM_VALUE_CMODE 87
#endif
#ifndef DEVLINK_PARAM_CMODE_DRIVERINIT
#define DEVLINK_PARAM_CMODE_DRIVERINIT 1
#endif
#ifndef DEVLINK_CMD_RELOAD
#define DEVLINK_CMD_RELOAD 37
#endif
#ifndef DEVLINK_CMD_PARAM_GET
#define DEVLINK_CMD_PARAM_GET 38
#endif
#ifndef DEVLINK_CMD_PARAM_SET
#define DEVLINK_CMD_PARAM_SET 39
#endif
#ifndef NLA_FLAG
#define NLA_FLAG 6
#endif

/* Add/remove MAC address through Netlink */
struct mlx5_nl_mac_addr {
	struct rte_ether_addr (*mac)[];
	/**< MAC address handled by the device. */
	int mac_n; /**< Number of addresses in the array. */
};

RTE_ATOMIC(uint32_t) atomic_sn;

/* Generate Netlink sequence number. */
#define MLX5_NL_SN_GENERATE (rte_atomic_fetch_add_explicit(&atomic_sn, 1, \
	rte_memory_order_relaxed) + 1)

/**
 * Opens a Netlink socket.
 *
 * @param protocol
 *   Netlink protocol (e.g. NETLINK_ROUTE, NETLINK_RDMA).
 * @param groups
 *   Groups to listen (e.g. RTMGRP_LINK), can be 0.
 *
 * @return
 *   A file descriptor on success, a negative errno value otherwise and
 *   rte_errno is set.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_init)
int
mlx5_nl_init(int protocol, int groups)
{
	int fd;
	int buf_size;
	socklen_t opt_size;
	struct sockaddr_nl local = {
		.nl_family = AF_NETLINK,
		.nl_groups = groups,
	};
	int ret;

	fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, protocol);
	if (fd == -1) {
		rte_errno = errno;
		return -rte_errno;
	}
	opt_size = sizeof(buf_size);
	ret = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buf_size, &opt_size);
	if (ret == -1) {
		rte_errno = errno;
		goto error;
	}
	DRV_LOG(DEBUG, "Netlink socket send buffer: %d", buf_size);
	if (buf_size < MLX5_SEND_BUF_SIZE) {
		ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
				 &buf_size, sizeof(buf_size));
		if (ret == -1) {
			rte_errno = errno;
			goto error;
		}
	}
	opt_size = sizeof(buf_size);
	ret = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buf_size, &opt_size);
	if (ret == -1) {
		rte_errno = errno;
		goto error;
	}
	DRV_LOG(DEBUG, "Netlink socket recv buffer: %d", buf_size);
	if (buf_size < MLX5_RECV_BUF_SIZE) {
		ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
				 &buf_size, sizeof(buf_size));
		if (ret == -1) {
			rte_errno = errno;
			goto error;
		}
	}
	ret = bind(fd, (struct sockaddr *)&local, sizeof(local));
	if (ret == -1) {
		rte_errno = errno;
		goto error;
	}
	return fd;
error:
	close(fd);
	return -rte_errno;
}

/**
 * Send a request message to the kernel on the Netlink socket.
 *
 * @param[in] nlsk_fd
 *   Netlink socket file descriptor.
 * @param[in] nh
 *   The Netlink message send to the kernel.
 * @param[in] ssn
 *   Sequence number.
 * @param[in] req
 *   Pointer to the request structure.
 * @param[in] len
 *   Length of the request in bytes.
 *
 * @return
 *   The number of sent bytes on success, a negative errno value otherwise and
 *   rte_errno is set.
 */
static int
mlx5_nl_request(int nlsk_fd, struct nlmsghdr *nh, uint32_t sn, void *req,
		int len)
{
	struct sockaddr_nl sa = {
		.nl_family = AF_NETLINK,
	};
	struct iovec iov[2] = {
		{ .iov_base = nh, .iov_len = sizeof(*nh), },
		{ .iov_base = req, .iov_len = len, },
	};
	struct msghdr msg = {
		.msg_name = &sa,
		.msg_namelen = sizeof(sa),
		.msg_iov = iov,
		.msg_iovlen = 2,
	};
	int send_bytes;

	nh->nlmsg_pid = 0; /* communication with the kernel uses pid 0 */
	nh->nlmsg_seq = sn;
	send_bytes = sendmsg(nlsk_fd, &msg, 0);
	if (send_bytes < 0) {
		rte_errno = errno;
		return -rte_errno;
	}
	return send_bytes;
}

/**
 * Send a message to the kernel on the Netlink socket.
 *
 * @param[in] nlsk_fd
 *   The Netlink socket file descriptor used for communication.
 * @param[in] nh
 *   The Netlink message send to the kernel.
 * @param[in] sn
 *   Sequence number.
 *
 * @return
 *   The number of sent bytes on success, a negative errno value otherwise and
 *   rte_errno is set.
 */
static int
mlx5_nl_send(int nlsk_fd, struct nlmsghdr *nh, uint32_t sn)
{
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
	nh->nlmsg_seq = sn;
	send_bytes = sendmsg(nlsk_fd, &msg, 0);
	if (send_bytes < 0) {
		rte_errno = errno;
		return -rte_errno;
	}
	return send_bytes;
}

/**
 * Receive a message from the kernel on the Netlink socket, following
 * mlx5_nl_send().
 *
 * @param[in] nlsk_fd
 *   The Netlink socket file descriptor used for communication.
 * @param[in] sn
 *   Sequence number.
 * @param[in] cb
 *   The callback function to call for each Netlink message received.
 * @param[in, out] arg
 *   Custom arguments for the callback.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_nl_recv(int nlsk_fd, uint32_t sn, int (*cb)(struct nlmsghdr *, void *arg),
	     void *arg)
{
	struct sockaddr_nl sa;
	struct iovec iov;
	struct msghdr msg = {
		.msg_name = &sa,
		.msg_namelen = sizeof(sa),
		.msg_iov = &iov,
		/* One message at a time */
		.msg_iovlen = 1,
	};
	void *buf = NULL;
	int multipart = 0;
	int ret = 0;

	do {
		struct nlmsghdr *nh;
		int recv_bytes;

		do {
			/* Query length of incoming message. */
			iov.iov_base = NULL;
			iov.iov_len = 0;
			recv_bytes = recvmsg(nlsk_fd, &msg,
					     MSG_PEEK | MSG_TRUNC);
			if (recv_bytes < 0) {
				rte_errno = errno;
				ret = -rte_errno;
				goto exit;
			}
			if (recv_bytes == 0) {
				rte_errno = ENODATA;
				ret = -rte_errno;
				goto exit;
			}
			/* Allocate buffer to fetch the message. */
			if (recv_bytes < MLX5_RECV_BUF_SIZE)
				recv_bytes = MLX5_RECV_BUF_SIZE;
			mlx5_free(buf);
			buf = mlx5_malloc(0, recv_bytes, 0, SOCKET_ID_ANY);
			if (!buf) {
				rte_errno = ENOMEM;
				ret = -rte_errno;
				goto exit;
			}
			/* Fetch the message. */
			iov.iov_base = buf;
			iov.iov_len = recv_bytes;
			recv_bytes = recvmsg(nlsk_fd, &msg, 0);
			if (recv_bytes == -1) {
				rte_errno = errno;
				ret = -rte_errno;
				goto exit;
			}
			nh = (struct nlmsghdr *)buf;
		} while (nh->nlmsg_seq != sn);
		for (;
		     NLMSG_OK(nh, (unsigned int)recv_bytes);
		     nh = NLMSG_NEXT(nh, recv_bytes)) {
			if (nh->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *err_data = NLMSG_DATA(nh);

				if (err_data->error < 0) {
					rte_errno = -err_data->error;
					ret = -rte_errno;
					goto exit;
				}
				/* Ack message. */
				ret = 0;
				goto exit;
			}
			/* Multi-part msgs and their trailing DONE message. */
			if (nh->nlmsg_flags & NLM_F_MULTI) {
				if (nh->nlmsg_type == NLMSG_DONE) {
					ret =  0;
					goto exit;
				}
				multipart = 1;
			}
			if (cb) {
				ret = cb(nh, arg);
				if (ret < 0)
					goto exit;
			}
		}
	} while (multipart);
exit:
	mlx5_free(buf);
	return ret;
}

/**
 * Parse Netlink message to retrieve the bridge MAC address.
 *
 * @param nh
 *   Pointer to Netlink Message Header.
 * @param arg
 *   PMD data register with this callback.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_nl_mac_addr_cb(struct nlmsghdr *nh, void *arg)
{
	struct mlx5_nl_mac_addr *data = arg;
	struct ndmsg *r = NLMSG_DATA(nh);
	struct rtattr *attribute;
	int len;

	len = nh->nlmsg_len - NLMSG_LENGTH(sizeof(*r));
	for (attribute = MLX5_NDA_RTA(r);
	     RTA_OK(attribute, len);
	     attribute = RTA_NEXT(attribute, len)) {
		if (attribute->rta_type == NDA_LLADDR) {
			if (data->mac_n == MLX5_MAX_MAC_ADDRESSES) {
				DRV_LOG(WARNING,
					"not enough room to finalize the"
					" request");
				rte_errno = ENOMEM;
				return -rte_errno;
			}
#ifdef RTE_LIBRTE_MLX5_DEBUG
			char m[RTE_ETHER_ADDR_FMT_SIZE];

			rte_ether_format_addr(m, RTE_ETHER_ADDR_FMT_SIZE,
					      RTA_DATA(attribute));
			DRV_LOG(DEBUG, "bridge MAC address %s", m);
#endif
			memcpy(&(*data->mac)[data->mac_n++],
			       RTA_DATA(attribute), RTE_ETHER_ADDR_LEN);
		}
	}
	return 0;
}

/**
 * Get bridge MAC addresses.
 *
 * @param[in] nlsk_fd
 *   Netlink socket file descriptor.
 * @param[in] iface_idx
 *   Net device interface index.
 * @param mac[out]
 *   Pointer to the array table of MAC addresses to fill.
 *   Its size should be of MLX5_MAX_MAC_ADDRESSES.
 * @param mac_n[out]
 *   Number of entries filled in MAC array.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_nl_mac_addr_list(int nlsk_fd, unsigned int iface_idx,
		      struct rte_ether_addr (*mac)[], int *mac_n)
{
	struct {
		struct nlmsghdr	hdr;
		struct ifinfomsg ifm;
	} req = {
		.hdr = {
			.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
			.nlmsg_type = RTM_GETNEIGH,
			.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST,
		},
		.ifm = {
			.ifi_family = PF_BRIDGE,
			.ifi_index = iface_idx,
		},
	};
	struct mlx5_nl_mac_addr data = {
		.mac = mac,
		.mac_n = 0,
	};
	uint32_t sn = MLX5_NL_SN_GENERATE;
	int ret;

	if (nlsk_fd == -1)
		return 0;
	ret = mlx5_nl_request(nlsk_fd, &req.hdr, sn, &req.ifm,
			      sizeof(struct ifinfomsg));
	if (ret < 0)
		goto error;
	ret = mlx5_nl_recv(nlsk_fd, sn, mlx5_nl_mac_addr_cb, &data);
	if (ret < 0)
		goto error;
	*mac_n = data.mac_n;
	return 0;
error:
	DRV_LOG(DEBUG, "Interface %u cannot retrieve MAC address list %s",
		iface_idx, strerror(rte_errno));
	return -rte_errno;
}

/**
 * Modify the MAC address neighbour table with Netlink.
 *
 * @param[in] nlsk_fd
 *   Netlink socket file descriptor.
 * @param[in] iface_idx
 *   Net device interface index.
 * @param mac
 *   MAC address to consider.
 * @param add
 *   1 to add the MAC address, 0 to remove the MAC address.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_nl_mac_addr_modify(int nlsk_fd, unsigned int iface_idx,
			struct rte_ether_addr *mac, int add)
{
	struct {
		struct nlmsghdr hdr;
		struct ndmsg ndm;
		struct rtattr rta;
		uint8_t buffer[RTE_ETHER_ADDR_LEN];
	} req = {
		.hdr = {
			.nlmsg_len = NLMSG_LENGTH(sizeof(struct ndmsg)),
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE |
				NLM_F_EXCL | NLM_F_ACK,
			.nlmsg_type = add ? RTM_NEWNEIGH : RTM_DELNEIGH,
		},
		.ndm = {
			.ndm_family = PF_BRIDGE,
			.ndm_state = NUD_NOARP | NUD_PERMANENT,
			.ndm_ifindex = iface_idx,
			.ndm_flags = NTF_SELF,
		},
		.rta = {
			.rta_type = NDA_LLADDR,
			.rta_len = RTA_LENGTH(RTE_ETHER_ADDR_LEN),
		},
	};
	uint32_t sn = MLX5_NL_SN_GENERATE;
	int ret;

	if (nlsk_fd == -1)
		return 0;
	memcpy(RTA_DATA(&req.rta), mac, RTE_ETHER_ADDR_LEN);
	req.hdr.nlmsg_len = NLMSG_ALIGN(req.hdr.nlmsg_len) +
		RTA_ALIGN(req.rta.rta_len);
	ret = mlx5_nl_send(nlsk_fd, &req.hdr, sn);
	if (ret < 0)
		goto error;
	ret = mlx5_nl_recv(nlsk_fd, sn, NULL, NULL);
	if (ret < 0)
		goto error;
	return 0;
error:
#ifdef RTE_LIBRTE_MLX5_DEBUG
	{
		char m[RTE_ETHER_ADDR_FMT_SIZE];

		rte_ether_format_addr(m, RTE_ETHER_ADDR_FMT_SIZE, mac);
		DRV_LOG(DEBUG,
			"Interface %u cannot %s MAC address %s %s",
			iface_idx,
			add ? "add" : "remove", m, strerror(rte_errno));
	}
#endif
	return -rte_errno;
}

/**
 * Modify the VF MAC address neighbour table with Netlink.
 *
 * @param[in] nlsk_fd
 *   Netlink socket file descriptor.
 * @param[in] iface_idx
 *   Net device interface index.
 * @param mac
 *    MAC address to consider.
 * @param vf_index
 *    VF index.
 *
 * @return
 *    0 on success, a negative errno value otherwise and rte_errno is set.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_vf_mac_addr_modify)
int
mlx5_nl_vf_mac_addr_modify(int nlsk_fd, unsigned int iface_idx,
			   struct rte_ether_addr *mac, int vf_index)
{
	int ret;
	struct {
		struct nlmsghdr hdr;
		struct ifinfomsg ifm;
		struct rtattr vf_list_rta;
		struct rtattr vf_info_rta;
		struct rtattr vf_mac_rta;
		struct ifla_vf_mac ivm;
	} req = {
		.hdr = {
			.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK,
			.nlmsg_type = RTM_BASE,
		},
		.ifm = {
			.ifi_index = iface_idx,
		},
		.vf_list_rta = {
			.rta_type = IFLA_VFINFO_LIST,
			.rta_len = RTA_ALIGN(RTA_LENGTH(0)),
		},
		.vf_info_rta = {
			.rta_type = IFLA_VF_INFO,
			.rta_len = RTA_ALIGN(RTA_LENGTH(0)),
		},
		.vf_mac_rta = {
			.rta_type = IFLA_VF_MAC,
		},
	};
	struct ifla_vf_mac ivm = {
		.vf = vf_index,
	};
	uint32_t sn = MLX5_NL_SN_GENERATE;

	memcpy(&ivm.mac, mac, RTE_ETHER_ADDR_LEN);
	memcpy(RTA_DATA(&req.vf_mac_rta), &ivm, sizeof(ivm));

	req.vf_mac_rta.rta_len = RTA_LENGTH(sizeof(ivm));
	req.hdr.nlmsg_len = NLMSG_ALIGN(req.hdr.nlmsg_len) +
		RTA_ALIGN(req.vf_list_rta.rta_len) +
		RTA_ALIGN(req.vf_info_rta.rta_len) +
		RTA_ALIGN(req.vf_mac_rta.rta_len);
	req.vf_list_rta.rta_len = RTE_PTR_DIFF(NLMSG_TAIL(&req.hdr),
					       &req.vf_list_rta);
	req.vf_info_rta.rta_len = RTE_PTR_DIFF(NLMSG_TAIL(&req.hdr),
					       &req.vf_info_rta);

	if (nlsk_fd < 0)
		return -1;
	ret = mlx5_nl_send(nlsk_fd, &req.hdr, sn);
	if (ret < 0)
		goto error;
	ret = mlx5_nl_recv(nlsk_fd, sn, NULL, NULL);
	if (ret < 0)
		goto error;
	return 0;
error:
	DRV_LOG(ERR,
		"representor %u cannot set VF MAC address "
		RTE_ETHER_ADDR_PRT_FMT " : %s",
		vf_index,
		RTE_ETHER_ADDR_BYTES(mac),
		strerror(rte_errno));
	return -rte_errno;
}

/**
 * Add a MAC address.
 *
 * @param[in] nlsk_fd
 *   Netlink socket file descriptor.
 * @param[in] iface_idx
 *   Net device interface index.
 * @param mac_own
 *   BITFIELD_DECLARE array to store the mac.
 * @param mac
 *   MAC address to register.
 * @param index
 *   MAC address index.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_mac_addr_add)
int
mlx5_nl_mac_addr_add(int nlsk_fd, unsigned int iface_idx,
		     uint64_t *mac_own, struct rte_ether_addr *mac,
		     uint32_t index)
{
	int ret;

	ret = mlx5_nl_mac_addr_modify(nlsk_fd, iface_idx, mac, 1);
	if (!ret) {
		MLX5_ASSERT(index < MLX5_MAX_MAC_ADDRESSES);
		if (index >= MLX5_MAX_MAC_ADDRESSES)
			return -EINVAL;

		BITFIELD_SET(mac_own, index);
	}
	if (ret == -EEXIST)
		return 0;
	return ret;
}

/**
 * Remove a MAC address.
 *
 * @param[in] nlsk_fd
 *   Netlink socket file descriptor.
 * @param[in] iface_idx
 *   Net device interface index.
 * @param mac_own
 *   BITFIELD_DECLARE array to store the mac.
 * @param mac
 *   MAC address to remove.
 * @param index
 *   MAC address index.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_mac_addr_remove)
int
mlx5_nl_mac_addr_remove(int nlsk_fd, unsigned int iface_idx, uint64_t *mac_own,
			struct rte_ether_addr *mac, uint32_t index)
{
	MLX5_ASSERT(index < MLX5_MAX_MAC_ADDRESSES);
	if (index >= MLX5_MAX_MAC_ADDRESSES)
		return -EINVAL;

	BITFIELD_RESET(mac_own, index);
	return mlx5_nl_mac_addr_modify(nlsk_fd, iface_idx, mac, 0);
}

/**
 * Synchronize Netlink bridge table to the internal table.
 *
 * @param[in] nlsk_fd
 *   Netlink socket file descriptor.
 * @param[in] iface_idx
 *   Net device interface index.
 * @param mac_addrs
 *   Mac addresses array to sync.
 * @param n
 *   @p mac_addrs array size.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_mac_addr_sync)
void
mlx5_nl_mac_addr_sync(int nlsk_fd, unsigned int iface_idx,
		      struct rte_ether_addr *mac_addrs, int n)
{
	struct rte_ether_addr macs[n];
	int macs_n = 0;
	int i;
	int ret;

	memset(macs, 0, n * sizeof(macs[0]));
	ret = mlx5_nl_mac_addr_list(nlsk_fd, iface_idx, &macs, &macs_n);
	if (ret)
		return;
	for (i = 0; i != macs_n; ++i) {
		int j;

		/* Verify the address is not in the array yet. */
		for (j = 0; j != n; ++j)
			if (rte_is_same_ether_addr(&macs[i], &mac_addrs[j]))
				break;
		if (j != n)
			continue;
		if (rte_is_multicast_ether_addr(&macs[i])) {
			/* Find the first entry available. */
			for (j = MLX5_MAX_UC_MAC_ADDRESSES; j != n; ++j) {
				if (rte_is_zero_ether_addr(&mac_addrs[j])) {
					mac_addrs[j] = macs[i];
					break;
				}
			}
		} else {
			/* Find the first entry available. */
			for (j = 0; j != MLX5_MAX_UC_MAC_ADDRESSES; ++j) {
				if (rte_is_zero_ether_addr(&mac_addrs[j])) {
					mac_addrs[j] = macs[i];
					break;
				}
			}
		}
	}
}

/**
 * Flush all added MAC addresses.
 *
 * @param[in] nlsk_fd
 *   Netlink socket file descriptor.
 * @param[in] iface_idx
 *   Net device interface index.
 * @param[in] mac_addrs
 *   Mac addresses array to flush.
 * @param n
 *   @p mac_addrs array size.
 * @param mac_own
 *   BITFIELD_DECLARE array to store the mac.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_mac_addr_flush)
void
mlx5_nl_mac_addr_flush(int nlsk_fd, unsigned int iface_idx,
		       struct rte_ether_addr *mac_addrs, int n,
		       uint64_t *mac_own)
{
	int i;

	if (n <= 0 || n > MLX5_MAX_MAC_ADDRESSES)
		return;

	for (i = n - 1; i >= 0; --i) {
		struct rte_ether_addr *m = &mac_addrs[i];

		if (BITFIELD_ISSET(mac_own, i))
			mlx5_nl_mac_addr_remove(nlsk_fd, iface_idx, mac_own, m,
						i);
	}
}

/**
 * Enable promiscuous / all multicast mode through Netlink.
 *
 * @param[in] nlsk_fd
 *   Netlink socket file descriptor.
 * @param[in] iface_idx
 *   Net device interface index.
 * @param flags
 *   IFF_PROMISC for promiscuous, IFF_ALLMULTI for allmulti.
 * @param enable
 *   Nonzero to enable, disable otherwise.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_nl_device_flags(int nlsk_fd, unsigned int iface_idx, uint32_t flags,
		     int enable)
{
	struct {
		struct nlmsghdr hdr;
		struct ifinfomsg ifi;
	} req = {
		.hdr = {
			.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
			.nlmsg_type = RTM_NEWLINK,
			.nlmsg_flags = NLM_F_REQUEST,
		},
		.ifi = {
			.ifi_flags = enable ? flags : 0,
			.ifi_change = flags,
			.ifi_index = iface_idx,
		},
	};
	uint32_t sn = MLX5_NL_SN_GENERATE;
	int ret;

	MLX5_ASSERT(!(flags & ~(IFF_PROMISC | IFF_ALLMULTI)));
	if (nlsk_fd < 0)
		return 0;
	ret = mlx5_nl_send(nlsk_fd, &req.hdr, sn);
	if (ret < 0)
		return ret;
	return 0;
}

/**
 * Enable promiscuous mode through Netlink.
 *
 * @param[in] nlsk_fd
 *   Netlink socket file descriptor.
 * @param[in] iface_idx
 *   Net device interface index.
 * @param enable
 *   Nonzero to enable, disable otherwise.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_promisc)
int
mlx5_nl_promisc(int nlsk_fd, unsigned int iface_idx, int enable)
{
	int ret = mlx5_nl_device_flags(nlsk_fd, iface_idx, IFF_PROMISC, enable);

	if (ret)
		DRV_LOG(DEBUG,
			"Interface %u cannot %s promisc mode: Netlink error %s",
			iface_idx, enable ? "enable" : "disable",
			strerror(rte_errno));
	return ret;
}

/**
 * Enable all multicast mode through Netlink.
 *
 * @param[in] nlsk_fd
 *   Netlink socket file descriptor.
 * @param[in] iface_idx
 *   Net device interface index.
 * @param enable
 *   Nonzero to enable, disable otherwise.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_allmulti)
int
mlx5_nl_allmulti(int nlsk_fd, unsigned int iface_idx, int enable)
{
	int ret = mlx5_nl_device_flags(nlsk_fd, iface_idx, IFF_ALLMULTI,
				       enable);

	if (ret)
		DRV_LOG(DEBUG,
			"Interface %u cannot %s allmulti : Netlink error %s",
			iface_idx, enable ? "enable" : "disable",
			strerror(rte_errno));
	return ret;
}

/**
 * Process network interface information from Netlink message.
 *
 * @param nh
 *   Pointer to Netlink message header.
 * @param arg
 *   Opaque data pointer for this callback.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_nl_cmdget_cb(struct nlmsghdr *nh, void *arg)
{
	struct mlx5_nl_port_info *data = arg;
	struct mlx5_nl_port_info local = {
		.flags = 0,
	};
	size_t off = NLMSG_HDRLEN;

	if (nh->nlmsg_type !=
	    RDMA_NL_GET_TYPE(RDMA_NL_NLDEV, RDMA_NLDEV_CMD_GET) &&
	    nh->nlmsg_type !=
	    RDMA_NL_GET_TYPE(RDMA_NL_NLDEV, RDMA_NLDEV_CMD_PORT_GET))
		goto error;
	while (off < nh->nlmsg_len) {
		struct nlattr *na = (void *)((uintptr_t)nh + off);
		void *payload = (void *)((uintptr_t)na + NLA_HDRLEN);

		if (na->nla_len > nh->nlmsg_len - off)
			goto error;
		switch (na->nla_type) {
		case RDMA_NLDEV_ATTR_DEV_INDEX:
			local.ibindex = *(uint32_t *)payload;
			local.flags |= MLX5_NL_CMD_GET_IB_INDEX;
			break;
		case RDMA_NLDEV_ATTR_DEV_NAME:
			if (!strcmp(payload, data->name))
				local.flags |= MLX5_NL_CMD_GET_IB_NAME;
			break;
		case RDMA_NLDEV_ATTR_NDEV_INDEX:
			local.ifindex = *(uint32_t *)payload;
			local.flags |= MLX5_NL_CMD_GET_NET_INDEX;
			break;
		case RDMA_NLDEV_ATTR_PORT_INDEX:
			local.portnum = *(uint32_t *)payload;
			local.flags |= MLX5_NL_CMD_GET_PORT_INDEX;
			break;
		case RDMA_NLDEV_ATTR_PORT_STATE:
			local.state = *(uint8_t *)payload;
			local.flags |= MLX5_NL_CMD_GET_PORT_STATE;
			break;
		default:
			break;
		}
		off += NLA_ALIGN(na->nla_len);
	}
	/*
	 * It is possible to have multiple messages for all
	 * Infiniband devices in the system with appropriate name.
	 * So we should gather parameters locally and copy to
	 * query context only in case of coinciding device name.
	 */
	if (local.flags & MLX5_NL_CMD_GET_IB_NAME) {
		data->flags = local.flags;
		data->ibindex = local.ibindex;
		data->ifindex = local.ifindex;
		data->portnum = local.portnum;
		data->state = local.state;
	}
	return 0;
error:
	rte_errno = EINVAL;
	return -rte_errno;
}

/**
 * Get port info of network interface associated with some IB device.
 *
 * This is the only somewhat safe method to avoid resorting to heuristics
 * when faced with port representors. Unfortunately it requires at least
 * Linux 4.17.
 *
 * @param nl
 *   Netlink socket of the RDMA kind (NETLINK_RDMA).
 * @param[in] pindex
 *   IB device port index, starting from 1
 * @param[out] data
 *   Pointer to port info.
 * @return
 *   0 on success, negative on error and rte_errno is set.
 */
static int
mlx5_nl_port_info(int nl, uint32_t pindex, struct mlx5_nl_port_info *data)
{
	union {
		struct nlmsghdr nh;
		uint8_t buf[NLMSG_HDRLEN +
			    NLA_HDRLEN + NLA_ALIGN(sizeof(data->ibindex)) +
			    NLA_HDRLEN + NLA_ALIGN(sizeof(pindex))];
	} req = {
		.nh = {
			.nlmsg_len = NLMSG_LENGTH(0),
			.nlmsg_type = RDMA_NL_GET_TYPE(RDMA_NL_NLDEV,
						       RDMA_NLDEV_CMD_GET),
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_DUMP,
		},
	};
	struct nlattr *na;
	uint32_t sn = MLX5_NL_SN_GENERATE;
	int ret;

	if (data->ibindex == UINT32_MAX) {
		ret = mlx5_nl_send(nl, &req.nh, sn);
		if (ret < 0)
			return ret;
		ret = mlx5_nl_recv(nl, sn, mlx5_nl_cmdget_cb, data);
		if (ret < 0)
			return ret;
		if (!(data->flags & MLX5_NL_CMD_GET_IB_NAME) ||
		    !(data->flags & MLX5_NL_CMD_GET_IB_INDEX))
			goto error;
		data->flags = 0;
	}
	sn = MLX5_NL_SN_GENERATE;
	req.nh.nlmsg_type = RDMA_NL_GET_TYPE(RDMA_NL_NLDEV,
					     RDMA_NLDEV_CMD_PORT_GET);
	req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(req.buf) - NLMSG_HDRLEN);
	na = (void *)((uintptr_t)req.buf + NLMSG_HDRLEN);
	na->nla_len = NLA_HDRLEN + sizeof(data->ibindex);
	na->nla_type = RDMA_NLDEV_ATTR_DEV_INDEX;
	memcpy((void *)((uintptr_t)na + NLA_HDRLEN),
	       &data->ibindex, sizeof(data->ibindex));
	na = (void *)((uintptr_t)na + NLA_ALIGN(na->nla_len));
	na->nla_len = NLA_HDRLEN + sizeof(pindex);
	na->nla_type = RDMA_NLDEV_ATTR_PORT_INDEX;
	memcpy((void *)((uintptr_t)na + NLA_HDRLEN),
	       &pindex, sizeof(pindex));
	ret = mlx5_nl_send(nl, &req.nh, sn);
	if (ret < 0)
		return ret;
	ret = mlx5_nl_recv(nl, sn, mlx5_nl_cmdget_cb, data);
	if (ret < 0)
		return ret;
	if (!(data->flags & MLX5_NL_CMD_GET_IB_NAME) ||
	    !(data->flags & MLX5_NL_CMD_GET_IB_INDEX) ||
	    !(data->flags & MLX5_NL_CMD_GET_NET_INDEX) ||
	    !data->ifindex)
		goto error;
	return 0;
error:
	rte_errno = ENODEV;
	return -rte_errno;
}

/**
 * Get index of network interface associated with some IB device.
 *
 * This is the only somewhat safe method to avoid resorting to heuristics
 * when faced with port representors. Unfortunately it requires at least
 * Linux 4.17.
 *
 * @param nl
 *   Netlink socket of the RDMA kind (NETLINK_RDMA).
 * @param[in] name
 *   IB device name.
 * @param[in] pindex
 *   IB device port index, starting from 1
 * @param[in] dev_info
 *   Cached mlx5 device information.
 * @return
 *   A valid (nonzero) interface index on success, 0 otherwise and rte_errno
 *   is set.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_ifindex)
unsigned int
mlx5_nl_ifindex(int nl, const char *name, uint32_t pindex, struct mlx5_dev_info *dev_info)
{
	int ret;

	struct mlx5_nl_port_info data = {
			.ifindex = 0,
			.name = name,
			.ibindex = UINT32_MAX,
			.flags = 0,
	};

	if (dev_info->probe_opt && !strcmp(name, dev_info->ibname)) {
		if (dev_info->port_info && pindex <= dev_info->port_num &&
		    dev_info->port_info[pindex].valid) {
			if (!dev_info->port_info[pindex].ifindex)
				rte_errno = ENODEV;
			return dev_info->port_info[pindex].ifindex;
		}
		if (dev_info->port_num)
			data.ibindex = dev_info->ibindex;
	}

	ret = mlx5_nl_port_info(nl, pindex, &data);

	if (dev_info->probe_opt && !strcmp(dev_info->ibname, name)) {
		if ((!ret || ret == -ENODEV) && dev_info->port_info &&
		    pindex <= dev_info->port_num) {
			if (!ret)
				dev_info->port_info[pindex].ifindex = data.ifindex;
			/* -ENODEV means the pindex is unused but still valid case */
			dev_info->port_info[pindex].valid = 1;
		}
	}

	return ret ? 0 : data.ifindex;
}

/**
 * Get IB device port state.
 *
 * This is the only somewhat safe method to get info for port number >= 255.
 * Unfortunately it requires at least Linux 4.17.
 *
 * @param nl
 *   Netlink socket of the RDMA kind (NETLINK_RDMA).
 * @param[in] name
 *   IB device name.
 * @param[in] pindex
 *   IB device port index, starting from 1
 * @param[in] dev_info
 *   Cached mlx5 device information.
 * @return
 *   Port state (ibv_port_state) on success, negative on error
 *   and rte_errno is set.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_port_state)
int
mlx5_nl_port_state(int nl, const char *name, uint32_t pindex, struct mlx5_dev_info *dev_info)
{
	struct mlx5_nl_port_info data = {
			.state = 0,
			.name = name,
			.ibindex = UINT32_MAX,
	};

	if (dev_info && dev_info->probe_opt &&
	    !strcmp(name, dev_info->ibname) && dev_info->port_num)
		data.ibindex = dev_info->ibindex;
	if (mlx5_nl_port_info(nl, pindex, &data) < 0)
		return -rte_errno;
	if ((data.flags & MLX5_NL_CMD_GET_PORT_STATE) == 0) {
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	return (int)data.state;
}

/**
 * Get the number of physical ports of given IB device.
 *
 * @param nl
 *   Netlink socket of the RDMA kind (NETLINK_RDMA).
 * @param[in] name
 *   IB device name.
 * @param[in] dev_info
 *   Cached mlx5 device info.
 *
 * @return
 *   A valid (nonzero) number of ports on success, 0 otherwise
 *   and rte_errno is set.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_portnum)
unsigned int
mlx5_nl_portnum(int nl, const char *name, struct mlx5_dev_info *dev_info)
{
	struct mlx5_nl_port_info data = {
		.flags = 0,
		.name = name,
		.ifindex = 0,
		.portnum = 0,
	};
	struct nlmsghdr req = {
		.nlmsg_len = NLMSG_LENGTH(0),
		.nlmsg_type = RDMA_NL_GET_TYPE(RDMA_NL_NLDEV,
					       RDMA_NLDEV_CMD_GET),
		.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_DUMP,
	};
	uint32_t sn = MLX5_NL_SN_GENERATE;
	int ret, size;

	if (dev_info->probe_opt && dev_info->port_num &&
	    !strcmp(name, dev_info->ibname))
		return dev_info->port_num;

	ret = mlx5_nl_send(nl, &req, sn);
	if (ret < 0)
		return 0;
	ret = mlx5_nl_recv(nl, sn, mlx5_nl_cmdget_cb, &data);
	if (ret < 0)
		return 0;
	if (!(data.flags & MLX5_NL_CMD_GET_IB_NAME) ||
	    !(data.flags & MLX5_NL_CMD_GET_IB_INDEX) ||
	    !(data.flags & MLX5_NL_CMD_GET_PORT_INDEX)) {
		rte_errno = ENODEV;
		return 0;
	}
	if (!data.portnum) {
		rte_errno = EINVAL;
		return 0;
	}
	if (!dev_info->probe_opt)
		return data.portnum;
	MLX5_ASSERT(!strlen(dev_info->ibname));
	dev_info->port_num = data.portnum;
	dev_info->ibindex = data.ibindex;
	snprintf(dev_info->ibname, MLX5_FS_NAME_MAX, "%s", name);
	if (data.portnum > 1) {
		size = (data.portnum + 1) * sizeof(struct mlx5_port_nl_info);
		dev_info->port_info = mlx5_malloc(MLX5_MEM_ZERO | MLX5_MEM_RTE, size,
						  RTE_CACHE_LINE_SIZE,
						  SOCKET_ID_ANY);
		if (dev_info->port_info == NULL) {
			memset(dev_info, 0, sizeof(*dev_info));
			rte_errno = ENOMEM;
			return 0;
		}
	}
	return data.portnum;
}

/**
 * Analyze gathered port parameters via Netlink to recognize master
 * and representor devices for E-Switch configuration.
 *
 * @param[in] num_vf_set
 *   flag of presence of number of VFs port attribute.
 * @param[inout] switch_info
 *   Port information, including port name as a number and port name
 *   type if recognized
 *
 * @return
 *   master and representor flags are set in switch_info according to
 *   recognized parameters (if any).
 */
static void
mlx5_nl_check_switch_info(bool num_vf_set,
			  struct mlx5_switch_info *switch_info)
{
	switch (switch_info->name_type) {
	case MLX5_PHYS_PORT_NAME_TYPE_UNKNOWN:
		/*
		 * Name is not recognized, assume the master,
		 * check the number of VFs key presence.
		 */
		switch_info->master = num_vf_set;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_NOTSET:
		/*
		 * Name is not set, this assumes the legacy naming
		 * schema for master, just check if there is a
		 * number of VFs key.
		 */
		switch_info->master = num_vf_set;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_UPLINK:
		/* New uplink naming schema recognized. */
		switch_info->master = 1;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_LEGACY:
		/* Legacy representors naming schema. */
		switch_info->representor = !num_vf_set;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_PFHPF:
		/* Fallthrough */
	case MLX5_PHYS_PORT_NAME_TYPE_PFVF:
		/* Fallthrough */
	case MLX5_PHYS_PORT_NAME_TYPE_PFSF:
		/* New representors naming schema. */
		switch_info->representor = 1;
		break;
	}
}

/**
 * Process switch information from Netlink message.
 *
 * @param nh
 *   Pointer to Netlink message header.
 * @param arg
 *   Opaque data pointer for this callback.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_nl_switch_info_cb(struct nlmsghdr *nh, void *arg)
{
	struct mlx5_switch_info info = {
		.master = 0,
		.representor = 0,
		.name_type = MLX5_PHYS_PORT_NAME_TYPE_NOTSET,
		.port_name = 0,
		.switch_id = 0,
	};
	size_t off = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	bool switch_id_set = false;
	bool num_vf_set = false;
	int len;

	if (nh->nlmsg_type != RTM_NEWLINK)
		goto error;
	while (off < nh->nlmsg_len) {
		struct rtattr *ra = (void *)((uintptr_t)nh + off);
		void *payload = RTA_DATA(ra);
		unsigned int i;

		if (ra->rta_len > nh->nlmsg_len - off)
			goto error;
		switch (ra->rta_type) {
		case IFLA_NUM_VF:
			num_vf_set = true;
			break;
		case IFLA_PHYS_PORT_NAME:
			len = RTA_PAYLOAD(ra);
			/* Some kernels do not pad attributes with zero. */
			if (len > 0 && len < MLX5_PHYS_PORT_NAME_MAX) {
				char name[MLX5_PHYS_PORT_NAME_MAX];

				/*
				 * We can't just patch the message with padding
				 * zero - it might corrupt the following items
				 * in the message, we have to copy the string
				 * by attribute length and pad the copied one.
				 */
				memcpy(name, payload, len);
				name[len] = 0;
				mlx5_translate_port_name(name, &info);
			} else {
				info.name_type =
					MLX5_PHYS_PORT_NAME_TYPE_UNKNOWN;
			}
			break;
		case IFLA_PHYS_SWITCH_ID:
			info.switch_id = 0;
			for (i = 0; i < RTA_PAYLOAD(ra); ++i) {
				info.switch_id <<= 8;
				info.switch_id |= ((uint8_t *)payload)[i];
			}
			switch_id_set = true;
			break;
		}
		off += RTA_ALIGN(ra->rta_len);
	}
	if (switch_id_set) {
		/* We have some E-Switch configuration. */
		mlx5_nl_check_switch_info(num_vf_set, &info);
	}
	MLX5_ASSERT(!(info.master && info.representor));
	memcpy(arg, &info, sizeof(info));
	return 0;
error:
	rte_errno = EINVAL;
	return -rte_errno;
}

/**
 * Get switch information associated with network interface.
 *
 * @param nl
 *   Netlink socket of the ROUTE kind (NETLINK_ROUTE).
 * @param ifindex
 *   Network interface index.
 * @param[out] info
 *   Switch information object, populated in case of success.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_switch_info)
int
mlx5_nl_switch_info(int nl, unsigned int ifindex,
		    struct mlx5_switch_info *info)
{
	struct {
		struct nlmsghdr nh;
		struct ifinfomsg info;
		struct rtattr rta;
		uint32_t extmask;
	} req = {
		.nh = {
			.nlmsg_len = NLMSG_LENGTH
					(sizeof(req.info) +
					 RTA_LENGTH(sizeof(uint32_t))),
			.nlmsg_type = RTM_GETLINK,
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK,
		},
		.info = {
			.ifi_family = AF_UNSPEC,
			.ifi_index = ifindex,
		},
		.rta = {
			.rta_type = IFLA_EXT_MASK,
			.rta_len = RTA_LENGTH(sizeof(int32_t)),
		},
		.extmask = RTE_LE32(1),
	};
	uint32_t sn = MLX5_NL_SN_GENERATE;
	int ret;

	ret = mlx5_nl_send(nl, &req.nh, sn);
	if (ret >= 0)
		ret = mlx5_nl_recv(nl, sn, mlx5_nl_switch_info_cb, info);
	if (info->master && info->representor) {
		DRV_LOG(ERR, "ifindex %u device is recognized as master"
			     " and as representor", ifindex);
		rte_errno = ENODEV;
		ret = -rte_errno;
	}
	return ret;
}

/*
 * Delete VLAN network device by ifindex.
 *
 * @param[in] tcf
 *   Context object initialized by mlx5_nl_vlan_vmwa_init().
 * @param[in] ifindex
 *   Interface index of network device to delete.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_vlan_vmwa_delete)
void
mlx5_nl_vlan_vmwa_delete(struct mlx5_nl_vlan_vmwa_context *vmwa,
		      uint32_t ifindex)
{
	uint32_t sn = MLX5_NL_SN_GENERATE;
	int ret;
	struct {
		struct nlmsghdr nh;
		struct ifinfomsg info;
	} req = {
		.nh = {
			.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
			.nlmsg_type = RTM_DELLINK,
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK,
		},
		.info = {
			.ifi_family = AF_UNSPEC,
			.ifi_index = ifindex,
		},
	};

	if (ifindex) {
		ret = mlx5_nl_send(vmwa->nl_socket, &req.nh, sn);
		if (ret >= 0)
			ret = mlx5_nl_recv(vmwa->nl_socket, sn, NULL, NULL);
		if (ret < 0)
			DRV_LOG(WARNING, "netlink: error deleting VLAN WA"
				" ifindex %u, %d", ifindex, ret);
	}
}

/* Set of subroutines to build Netlink message. */
static struct nlattr *
nl_msg_tail(struct nlmsghdr *nlh)
{
	return (struct nlattr *)
		(((uint8_t *)nlh) + NLMSG_ALIGN(nlh->nlmsg_len));
}

static void
nl_attr_put(struct nlmsghdr *nlh, int type, const void *data, int alen)
{
	struct nlattr *nla = nl_msg_tail(nlh);

	nla->nla_type = type;
	nla->nla_len = NLMSG_ALIGN(sizeof(struct nlattr)) + alen;
	nlh->nlmsg_len += NLMSG_ALIGN(nla->nla_len);

	if (alen)
		memcpy((uint8_t *)nla + sizeof(struct nlattr), data, alen);
}

static struct nlattr *
nl_attr_nest_start(struct nlmsghdr *nlh, int type)
{
	struct nlattr *nest = (struct nlattr *)nl_msg_tail(nlh);

	nl_attr_put(nlh, type, NULL, 0);
	return nest;
}

static void
nl_attr_nest_end(struct nlmsghdr *nlh, struct nlattr *nest)
{
	nest->nla_len = (uint8_t *)nl_msg_tail(nlh) - (uint8_t *)nest;
}

/*
 * Create network VLAN device with specified VLAN tag.
 *
 * @param[in] tcf
 *   Context object initialized by mlx5_nl_vlan_vmwa_init().
 * @param[in] ifindex
 *   Base network interface index.
 * @param[in] tag
 *   VLAN tag for VLAN network device to create.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_vlan_vmwa_create)
uint32_t
mlx5_nl_vlan_vmwa_create(struct mlx5_nl_vlan_vmwa_context *vmwa,
			 uint32_t ifindex, uint16_t tag)
{
	struct nlmsghdr *nlh;
	struct ifinfomsg *ifm;
	char name[sizeof(MLX5_VMWA_VLAN_DEVICE_PFX) + 32];

	alignas(RTE_CACHE_LINE_SIZE)
	uint8_t buf[NLMSG_ALIGN(sizeof(struct nlmsghdr)) +
		    NLMSG_ALIGN(sizeof(struct ifinfomsg)) +
		    NLMSG_ALIGN(sizeof(struct nlattr)) * 8 +
		    NLMSG_ALIGN(sizeof(uint32_t)) +
		    NLMSG_ALIGN(sizeof(name)) +
		    NLMSG_ALIGN(sizeof("vlan")) +
		    NLMSG_ALIGN(sizeof(uint32_t)) +
		    NLMSG_ALIGN(sizeof(uint16_t)) + 16];
	struct nlattr *na_info;
	struct nlattr *na_vlan;
	uint32_t sn = MLX5_NL_SN_GENERATE;
	int ret;

	memset(buf, 0, sizeof(buf));
	nlh = (struct nlmsghdr *)buf;
	nlh->nlmsg_len = sizeof(struct nlmsghdr);
	nlh->nlmsg_type = RTM_NEWLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE |
			   NLM_F_EXCL | NLM_F_ACK;
	ifm = (struct ifinfomsg *)nl_msg_tail(nlh);
	nlh->nlmsg_len += sizeof(struct ifinfomsg);
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_type = 0;
	ifm->ifi_index = 0;
	ifm->ifi_flags = IFF_UP;
	ifm->ifi_change = 0xffffffff;
	nl_attr_put(nlh, IFLA_LINK, &ifindex, sizeof(ifindex));
	ret = snprintf(name, sizeof(name), "%s.%u.%u",
		       MLX5_VMWA_VLAN_DEVICE_PFX, ifindex, tag);
	nl_attr_put(nlh, IFLA_IFNAME, name, ret + 1);
	na_info = nl_attr_nest_start(nlh, IFLA_LINKINFO);
	nl_attr_put(nlh, IFLA_INFO_KIND, "vlan", sizeof("vlan"));
	na_vlan = nl_attr_nest_start(nlh, IFLA_INFO_DATA);
	nl_attr_put(nlh, IFLA_VLAN_ID, &tag, sizeof(tag));
	nl_attr_nest_end(nlh, na_vlan);
	nl_attr_nest_end(nlh, na_info);
	MLX5_ASSERT(sizeof(buf) >= nlh->nlmsg_len);
	ret = mlx5_nl_send(vmwa->nl_socket, nlh, sn);
	if (ret >= 0)
		ret = mlx5_nl_recv(vmwa->nl_socket, sn, NULL, NULL);
	if (ret < 0) {
		DRV_LOG(WARNING, "netlink: VLAN %s create failure (%d)", name,
			ret);
	}
	/* Try to get ifindex of created or pre-existing device. */
	ret = if_nametoindex(name);
	if (!ret) {
		DRV_LOG(WARNING, "VLAN %s failed to get index (%d)", name,
			errno);
		return 0;
	}
	return ret;
}

/**
 * Parse Netlink message to retrieve the general family ID.
 *
 * @param nh
 *   Pointer to Netlink Message Header.
 * @param arg
 *   PMD data register with this callback.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_nl_family_id_cb(struct nlmsghdr *nh, void *arg)
{

	struct nlattr *tail = RTE_PTR_ADD(nh, nh->nlmsg_len);
	struct nlattr *nla = RTE_PTR_ADD(nh, NLMSG_ALIGN(sizeof(*nh)) +
					NLMSG_ALIGN(sizeof(struct genlmsghdr)));

	for (; nla->nla_len && nla < tail;
	     nla = RTE_PTR_ADD(nla, NLMSG_ALIGN(nla->nla_len))) {
		if (nla->nla_type == CTRL_ATTR_FAMILY_ID) {
			*(uint16_t *)arg = *(uint16_t *)(nla + 1);
			return 0;
		}
	}
	return -EINVAL;
}

#define MLX5_NL_MAX_ATTR_SIZE 100
/**
 * Get generic netlink family ID.
 *
 * @param[in] nlsk_fd
 *   Netlink socket file descriptor.
 * @param[in] name
 *   The family name.
 *
 * @return
 *   ID >= 0 on success and @p enable is updated, a negative errno value
 *   otherwise and rte_errno is set.
 */
static int
mlx5_nl_generic_family_id_get(int nlsk_fd, const char *name)
{
	struct nlmsghdr *nlh;
	struct genlmsghdr *genl;
	uint32_t sn = MLX5_NL_SN_GENERATE;
	int name_size = strlen(name) + 1;
	int ret;
	uint16_t id = -1;
	uint8_t buf[NLMSG_ALIGN(sizeof(struct nlmsghdr)) +
		    NLMSG_ALIGN(sizeof(struct genlmsghdr)) +
		    NLMSG_ALIGN(sizeof(struct nlattr)) +
		    NLMSG_ALIGN(MLX5_NL_MAX_ATTR_SIZE)];

	memset(buf, 0, sizeof(buf));
	nlh = (struct nlmsghdr *)buf;
	nlh->nlmsg_len = sizeof(struct nlmsghdr);
	nlh->nlmsg_type = GENL_ID_CTRL;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	genl = (struct genlmsghdr *)nl_msg_tail(nlh);
	nlh->nlmsg_len += sizeof(struct genlmsghdr);
	genl->cmd = CTRL_CMD_GETFAMILY;
	genl->version = 1;
	nl_attr_put(nlh, CTRL_ATTR_FAMILY_NAME, name, name_size);
	ret = mlx5_nl_send(nlsk_fd, nlh, sn);
	if (ret >= 0)
		ret = mlx5_nl_recv(nlsk_fd, sn, mlx5_nl_family_id_cb, &id);
	if (ret < 0) {
		DRV_LOG(DEBUG, "Failed to get Netlink %s family ID: %d.", name,
			ret);
		return ret;
	}
	DRV_LOG(DEBUG, "Netlink \"%s\" family ID is %u.", name, id);
	return (int)id;
}

/**
 * Get Devlink family ID.
 *
 * @param[in] nlsk_fd
 *   Netlink socket file descriptor.
 *
 * @return
 *   ID >= 0 on success and @p enable is updated, a negative errno value
 *   otherwise and rte_errno is set.
 */

RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_devlink_family_id_get)
int
mlx5_nl_devlink_family_id_get(int nlsk_fd)
{
	return mlx5_nl_generic_family_id_get(nlsk_fd, DEVLINK_GENL_NAME);
}

/**
 * Parse Netlink message to retrieve the ROCE enable status.
 *
 * @param nh
 *   Pointer to Netlink Message Header.
 * @param arg
 *   PMD data register with this callback.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_nl_roce_cb(struct nlmsghdr *nh, void *arg)
{

	int ret = -EINVAL;
	int *enable = arg;
	struct nlattr *tail = RTE_PTR_ADD(nh, nh->nlmsg_len);
	struct nlattr *nla = RTE_PTR_ADD(nh, NLMSG_ALIGN(sizeof(*nh)) +
					NLMSG_ALIGN(sizeof(struct genlmsghdr)));

	while (nla->nla_len && nla < tail) {
		switch (nla->nla_type) {
		/* Expected nested attributes case. */
		case DEVLINK_ATTR_PARAM:
		case DEVLINK_ATTR_PARAM_VALUES_LIST:
		case DEVLINK_ATTR_PARAM_VALUE:
			ret = 0;
			nla += 1;
			break;
		case DEVLINK_ATTR_PARAM_VALUE_DATA:
			*enable = 1;
			return 0;
		default:
			nla = RTE_PTR_ADD(nla, NLMSG_ALIGN(nla->nla_len));
		}
	}
	*enable = 0;
	return ret;
}

/**
 * Get ROCE enable status through Netlink.
 *
 * @param[in] nlsk_fd
 *   Netlink socket file descriptor.
 * @param[in] family_id
 *   the Devlink family ID.
 * @param pci_addr
 *   The device PCI address.
 * @param[out] enable
 *   Where to store the enable status.
 *
 * @return
 *   0 on success and @p enable is updated, a negative errno value otherwise
 *   and rte_errno is set.
 */
int
mlx5_nl_enable_roce_get(int nlsk_fd, int family_id, const char *pci_addr,
			int *enable)
{
	struct nlmsghdr *nlh;
	struct genlmsghdr *genl;
	uint32_t sn = MLX5_NL_SN_GENERATE;
	int ret;
	int cur_en = 0;
	uint8_t buf[NLMSG_ALIGN(sizeof(struct nlmsghdr)) +
		    NLMSG_ALIGN(sizeof(struct genlmsghdr)) +
		    NLMSG_ALIGN(sizeof(struct nlattr)) * 4 +
		    NLMSG_ALIGN(MLX5_NL_MAX_ATTR_SIZE) * 4];

	memset(buf, 0, sizeof(buf));
	nlh = (struct nlmsghdr *)buf;
	nlh->nlmsg_len = sizeof(struct nlmsghdr);
	nlh->nlmsg_type = family_id;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	genl = (struct genlmsghdr *)nl_msg_tail(nlh);
	nlh->nlmsg_len += sizeof(struct genlmsghdr);
	genl->cmd = DEVLINK_CMD_PARAM_GET;
	genl->version = DEVLINK_GENL_VERSION;
	nl_attr_put(nlh, DEVLINK_ATTR_BUS_NAME, "pci", 4);
	nl_attr_put(nlh, DEVLINK_ATTR_DEV_NAME, pci_addr, strlen(pci_addr) + 1);
	nl_attr_put(nlh, DEVLINK_ATTR_PARAM_NAME, "enable_roce", 12);
	ret = mlx5_nl_send(nlsk_fd, nlh, sn);
	if (ret >= 0)
		ret = mlx5_nl_recv(nlsk_fd, sn, mlx5_nl_roce_cb, &cur_en);
	if (ret < 0) {
		DRV_LOG(DEBUG, "Failed to get ROCE enable on device %s: %d.",
			pci_addr, ret);
		return ret;
	}
	*enable = cur_en;
	DRV_LOG(DEBUG, "ROCE is %sabled for device \"%s\".",
		cur_en ? "en" : "dis", pci_addr);
	return ret;
}

/**
 * Reload mlx5 device kernel driver through Netlink.
 *
 * @param[in] nlsk_fd
 *   Netlink socket file descriptor.
 * @param[in] family_id
 *   the Devlink family ID.
 * @param pci_addr
 *   The device PCI address.
 * @param[out] enable
 *   The enable status to set.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_nl_driver_reload(int nlsk_fd, int family_id, const char *pci_addr)
{
	struct nlmsghdr *nlh;
	struct genlmsghdr *genl;
	uint32_t sn = MLX5_NL_SN_GENERATE;
	int ret;
	uint8_t buf[NLMSG_ALIGN(sizeof(struct nlmsghdr)) +
		    NLMSG_ALIGN(sizeof(struct genlmsghdr)) +
		    NLMSG_ALIGN(sizeof(struct nlattr)) * 2 +
		    NLMSG_ALIGN(MLX5_NL_MAX_ATTR_SIZE) * 2];

	memset(buf, 0, sizeof(buf));
	nlh = (struct nlmsghdr *)buf;
	nlh->nlmsg_len = sizeof(struct nlmsghdr);
	nlh->nlmsg_type = family_id;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	genl = (struct genlmsghdr *)nl_msg_tail(nlh);
	nlh->nlmsg_len += sizeof(struct genlmsghdr);
	genl->cmd = DEVLINK_CMD_RELOAD;
	genl->version = DEVLINK_GENL_VERSION;
	nl_attr_put(nlh, DEVLINK_ATTR_BUS_NAME, "pci", 4);
	nl_attr_put(nlh, DEVLINK_ATTR_DEV_NAME, pci_addr, strlen(pci_addr) + 1);
	ret = mlx5_nl_send(nlsk_fd, nlh, sn);
	if (ret >= 0)
		ret = mlx5_nl_recv(nlsk_fd, sn, NULL, NULL);
	if (ret < 0) {
		DRV_LOG(DEBUG, "Failed to reload %s device by Netlink - %d",
			pci_addr, ret);
		return ret;
	}
	DRV_LOG(DEBUG, "Device \"%s\" was reloaded by Netlink successfully.",
		pci_addr);
	return 0;
}

/**
 * Set ROCE enable status through Netlink.
 *
 * @param[in] nlsk_fd
 *   Netlink socket file descriptor.
 * @param[in] family_id
 *   the Devlink family ID.
 * @param pci_addr
 *   The device PCI address.
 * @param[out] enable
 *   The enable status to set.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_nl_enable_roce_set(int nlsk_fd, int family_id, const char *pci_addr,
			int enable)
{
	struct nlmsghdr *nlh;
	struct genlmsghdr *genl;
	uint32_t sn = MLX5_NL_SN_GENERATE;
	int ret;
	uint8_t buf[NLMSG_ALIGN(sizeof(struct nlmsghdr)) +
		    NLMSG_ALIGN(sizeof(struct genlmsghdr)) +
		    NLMSG_ALIGN(sizeof(struct nlattr)) * 6 +
		    NLMSG_ALIGN(MLX5_NL_MAX_ATTR_SIZE) * 6];
	uint8_t cmode = DEVLINK_PARAM_CMODE_DRIVERINIT;
	uint8_t ptype = NLA_FLAG;
;

	memset(buf, 0, sizeof(buf));
	nlh = (struct nlmsghdr *)buf;
	nlh->nlmsg_len = sizeof(struct nlmsghdr);
	nlh->nlmsg_type = family_id;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	genl = (struct genlmsghdr *)nl_msg_tail(nlh);
	nlh->nlmsg_len += sizeof(struct genlmsghdr);
	genl->cmd = DEVLINK_CMD_PARAM_SET;
	genl->version = DEVLINK_GENL_VERSION;
	nl_attr_put(nlh, DEVLINK_ATTR_BUS_NAME, "pci", 4);
	nl_attr_put(nlh, DEVLINK_ATTR_DEV_NAME, pci_addr, strlen(pci_addr) + 1);
	nl_attr_put(nlh, DEVLINK_ATTR_PARAM_NAME, "enable_roce", 12);
	nl_attr_put(nlh, DEVLINK_ATTR_PARAM_VALUE_CMODE, &cmode, sizeof(cmode));
	nl_attr_put(nlh, DEVLINK_ATTR_PARAM_TYPE, &ptype, sizeof(ptype));
	if (enable)
		nl_attr_put(nlh, DEVLINK_ATTR_PARAM_VALUE_DATA, NULL, 0);
	ret = mlx5_nl_send(nlsk_fd, nlh, sn);
	if (ret >= 0)
		ret = mlx5_nl_recv(nlsk_fd, sn, NULL, NULL);
	if (ret < 0) {
		DRV_LOG(DEBUG, "Failed to %sable ROCE for device %s by Netlink:"
			" %d.", enable ? "en" : "dis", pci_addr, ret);
		return ret;
	}
	DRV_LOG(DEBUG, "Device %s ROCE was %sabled by Netlink successfully.",
		pci_addr, enable ? "en" : "dis");
	/* Now, need to reload the driver. */
	return mlx5_nl_driver_reload(nlsk_fd, family_id, pci_addr);
}

/**
 * Try to parse a Netlink message as a link status update.
 *
 * @param hdr
 *  Netlink message header.
 * @param[out] ifindex
 *  Index of the updated interface.
 *
 * @return
 *  0 on success, negative on failure.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_parse_link_status_update)
int
mlx5_nl_parse_link_status_update(struct nlmsghdr *hdr, uint32_t *ifindex)
{
	struct ifinfomsg *info;

	switch (hdr->nlmsg_type) {
	case RTM_NEWLINK:
	case RTM_DELLINK:
	case RTM_GETLINK:
	case RTM_SETLINK:
		info = NLMSG_DATA(hdr);
		*ifindex = info->ifi_index;
		return 0;
	}
	return -1;
}

/**
 * Read pending events from a Netlink socket.
 *
 * @param nlsk_fd
 *  Netlink socket.
 * @param cb
 *  Callback invoked for each of the events.
 * @param cb_arg
 *  User data for the callback.
 *
 * @return
 *  0 on success, including the case when there are no events.
 *  Negative on failure and rte_errno is set.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_read_events)
int
mlx5_nl_read_events(int nlsk_fd, mlx5_nl_event_cb *cb, void *cb_arg)
{
	char buf[8192];
	struct sockaddr_nl addr;
	struct iovec iov = {
		.iov_base = buf,
		.iov_len = sizeof(buf),
	};
	struct msghdr msg = {
		.msg_name = &addr,
		.msg_namelen = sizeof(addr),
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};
	struct nlmsghdr *hdr;
	ssize_t size;

	while (1) {
		size = recvmsg(nlsk_fd, &msg, MSG_DONTWAIT);
		if (size < 0) {
			if (errno == EAGAIN)
				return 0;
			if (errno == EINTR)
				continue;
			DRV_LOG(DEBUG, "Failed to receive netlink message: %s",
				strerror(errno));
			rte_errno = errno;
			return -rte_errno;
		}
		hdr = (struct nlmsghdr *)buf;
		while (size >= (ssize_t)sizeof(*hdr)) {
			ssize_t msg_len = hdr->nlmsg_len;
			ssize_t data_len = msg_len - sizeof(*hdr);
			ssize_t aligned_len;

			if (data_len < 0) {
				DRV_LOG(DEBUG, "Netlink message too short");
				rte_errno = EINVAL;
				return -rte_errno;
			}
			aligned_len = NLMSG_ALIGN(msg_len);
			if (aligned_len > size) {
				DRV_LOG(DEBUG, "Netlink message too long");
				rte_errno = EINVAL;
				return -rte_errno;
			}
			cb(hdr, cb_arg);
			hdr = RTE_PTR_ADD(hdr, aligned_len);
			size -= aligned_len;
		}
	}
	return 0;
}

static int
mlx5_nl_esw_multiport_cb(struct nlmsghdr *nh, void *arg)
{

	int ret = -EINVAL;
	int *enable = arg;
	struct nlattr *tail = RTE_PTR_ADD(nh, nh->nlmsg_len);
	struct nlattr *nla = RTE_PTR_ADD(nh, NLMSG_ALIGN(sizeof(*nh)) +
					NLMSG_ALIGN(sizeof(struct genlmsghdr)));

	while (nla->nla_len && nla < tail) {
		switch (nla->nla_type) {
		/* Expected nested attributes case. */
		case DEVLINK_ATTR_PARAM:
		case DEVLINK_ATTR_PARAM_VALUES_LIST:
		case DEVLINK_ATTR_PARAM_VALUE:
			ret = 0;
			nla += 1;
			break;
		case DEVLINK_ATTR_PARAM_VALUE_DATA:
			*enable = 1;
			return 0;
		default:
			nla = RTE_PTR_ADD(nla, NLMSG_ALIGN(nla->nla_len));
		}
	}
	*enable = 0;
	return ret;
}

#define NL_ESW_MULTIPORT_PARAM "esw_multiport"

RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_devlink_esw_multiport_get)
int
mlx5_nl_devlink_esw_multiport_get(int nlsk_fd, int family_id, const char *pci_addr, int *enable)
{
	struct nlmsghdr *nlh;
	struct genlmsghdr *genl;
	uint32_t sn = MLX5_NL_SN_GENERATE;
	int ret;
	uint8_t buf[NLMSG_ALIGN(sizeof(struct nlmsghdr)) +
		    NLMSG_ALIGN(sizeof(struct genlmsghdr)) +
		    NLMSG_ALIGN(sizeof(struct nlattr)) * 4 +
		    NLMSG_ALIGN(MLX5_NL_MAX_ATTR_SIZE) * 4];

	memset(buf, 0, sizeof(buf));
	nlh = (struct nlmsghdr *)buf;
	nlh->nlmsg_len = sizeof(struct nlmsghdr);
	nlh->nlmsg_type = family_id;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	genl = (struct genlmsghdr *)nl_msg_tail(nlh);
	nlh->nlmsg_len += sizeof(struct genlmsghdr);
	genl->cmd = DEVLINK_CMD_PARAM_GET;
	genl->version = DEVLINK_GENL_VERSION;
	nl_attr_put(nlh, DEVLINK_ATTR_BUS_NAME, "pci", 4);
	nl_attr_put(nlh, DEVLINK_ATTR_DEV_NAME, pci_addr, strlen(pci_addr) + 1);
	nl_attr_put(nlh, DEVLINK_ATTR_PARAM_NAME,
		    NL_ESW_MULTIPORT_PARAM, sizeof(NL_ESW_MULTIPORT_PARAM));
	ret = mlx5_nl_send(nlsk_fd, nlh, sn);
	if (ret >= 0)
		ret = mlx5_nl_recv(nlsk_fd, sn, mlx5_nl_esw_multiport_cb, enable);
	if (ret < 0) {
		DRV_LOG(DEBUG, "Failed to get Multiport E-Switch enable on device %s: %d.",
			pci_addr, ret);
		return ret;
	}
	DRV_LOG(DEBUG, "Multiport E-Switch is %sabled for device \"%s\".",
		*enable ? "en" : "dis", pci_addr);
	return ret;
}

RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_rdma_monitor_init)
int
mlx5_nl_rdma_monitor_init(void)
{
	return mlx5_nl_init(NETLINK_RDMA, RDMA_NL_GROUP_NOTIFICATION);
}

RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_rdma_monitor_info_get)
void
mlx5_nl_rdma_monitor_info_get(struct nlmsghdr *hdr, struct mlx5_nl_port_info *data)
{
	size_t off = NLMSG_HDRLEN;
	uint8_t event_type = 0;

	if (hdr->nlmsg_type != RDMA_NL_GET_TYPE(RDMA_NL_NLDEV, RDMA_NLDEV_CMD_MONITOR))
		goto error;

	while (off < hdr->nlmsg_len) {
		struct nlattr *na = (void *)((uintptr_t)hdr + off);
		void *payload = (void *)((uintptr_t)na + NLA_HDRLEN);

		if (na->nla_len > hdr->nlmsg_len - off)
			goto error;
		switch (na->nla_type) {
		case RDMA_NLDEV_ATTR_EVENT_TYPE:
			event_type = *(uint8_t *)payload;
			if (event_type == RDMA_NETDEV_ATTACH_EVENT) {
				data->flags |= MLX5_NL_CMD_GET_EVENT_TYPE;
				data->event_type = MLX5_NL_RDMA_NETDEV_ATTACH_EVENT;
			} else if (event_type == RDMA_NETDEV_DETACH_EVENT) {
				data->flags |= MLX5_NL_CMD_GET_EVENT_TYPE;
				data->event_type = MLX5_NL_RDMA_NETDEV_DETACH_EVENT;
			}
			break;
		case RDMA_NLDEV_ATTR_DEV_INDEX:
			data->ibindex = *(uint32_t *)payload;
			data->flags |= MLX5_NL_CMD_GET_IB_INDEX;
			break;
		case RDMA_NLDEV_ATTR_PORT_INDEX:
			data->portnum = *(uint32_t *)payload;
			data->flags |= MLX5_NL_CMD_GET_PORT_INDEX;
			break;
		case RDMA_NLDEV_ATTR_NDEV_INDEX:
			data->ifindex = *(uint32_t *)payload;
			data->flags |= MLX5_NL_CMD_GET_NET_INDEX;
			break;
		default:
			DRV_LOG(DEBUG, "Unknown attribute[%d] found", na->nla_type);
			break;
		}
		off += NLA_ALIGN(na->nla_len);
	}

	return;

error:
	rte_errno = EINVAL;
}

static int
mlx5_nl_rdma_monitor_cap_get_cb(struct nlmsghdr *hdr, void *arg)
{
	size_t off = NLMSG_HDRLEN;
	uint8_t *cap = arg;

	if (hdr->nlmsg_type != RDMA_NL_GET_TYPE(RDMA_NL_NLDEV, RDMA_NLDEV_CMD_SYS_GET))
		goto error;

	*cap = 0;
	while (off < hdr->nlmsg_len) {
		struct nlattr *na = (void *)((uintptr_t)hdr + off);
		void *payload = (void *)((uintptr_t)na + NLA_HDRLEN);

		if (na->nla_len > hdr->nlmsg_len - off)
			goto error;
		switch (na->nla_type) {
		case RDMA_NLDEV_SYS_ATTR_MONITOR_MODE:
			*cap = *(uint8_t *)payload;
			return 0;
		default:
			break;
		}
		off += NLA_ALIGN(na->nla_len);
	}

	return 0;

error:
	return -EINVAL;
}

/**
 * Get RDMA monitor support in driver.
 *
 *
 * @param nl
 *   Netlink socket of the RDMA kind (NETLINK_RDMA).
 * @param[out] cap
 *   Pointer to port info.
 * @return
 *   0 on success, negative on error and rte_errno is set.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_nl_rdma_monitor_cap_get)
int
mlx5_nl_rdma_monitor_cap_get(int nl, uint8_t *cap)
{
	union {
		struct nlmsghdr nh;
		uint8_t buf[NLMSG_HDRLEN];
	} req = {
		.nh = {
			.nlmsg_len = NLMSG_LENGTH(0),
			.nlmsg_type = RDMA_NL_GET_TYPE(RDMA_NL_NLDEV,
						       RDMA_NLDEV_CMD_SYS_GET),
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK,
		},
	};
	uint32_t sn = MLX5_NL_SN_GENERATE;
	int ret;

	ret = mlx5_nl_send(nl, &req.nh, sn);
	if (ret < 0) {
		rte_errno = -ret;
		return ret;
	}
	ret = mlx5_nl_recv(nl, sn, mlx5_nl_rdma_monitor_cap_get_cb, cap);
	if (ret < 0) {
		rte_errno = -ret;
		return ret;
	}
	return 0;
}
