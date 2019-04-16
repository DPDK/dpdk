/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 6WIND S.A.
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <errno.h>
#include <linux/if_link.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <rdma/rdma_netlink.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <rte_errno.h>

#include "mlx5.h"
#include "mlx5_utils.h"

/* Size of the buffer to receive kernel messages */
#define MLX5_NL_BUF_SIZE (32 * 1024)
/* Send buffer size for the Netlink socket */
#define MLX5_SEND_BUF_SIZE 32768
/* Receive buffer size for the Netlink socket */
#define MLX5_RECV_BUF_SIZE 32768

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
#ifndef HAVE_RDMA_NLDEV_ATTR_NDEV_INDEX
#define RDMA_NLDEV_ATTR_NDEV_INDEX 50
#endif

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

/* Add/remove MAC address through Netlink */
struct mlx5_nl_mac_addr {
	struct ether_addr (*mac)[];
	/**< MAC address handled by the device. */
	int mac_n; /**< Number of addresses in the array. */
};

/** Data structure used by mlx5_nl_cmdget_cb(). */
struct mlx5_nl_ifindex_data {
	const char *name; /**< IB device name (in). */
	uint32_t ibindex; /**< IB device index (out). */
	uint32_t ifindex; /**< Network interface index (out). */
	uint32_t portnum; /**< IB device max port number. */
};

/**
 * Opens a Netlink socket.
 *
 * @param protocol
 *   Netlink protocol (e.g. NETLINK_ROUTE, NETLINK_RDMA).
 *
 * @return
 *   A file descriptor on success, a negative errno value otherwise and
 *   rte_errno is set.
 */
int
mlx5_nl_init(int protocol)
{
	int fd;
	int sndbuf_size = MLX5_SEND_BUF_SIZE;
	int rcvbuf_size = MLX5_RECV_BUF_SIZE;
	struct sockaddr_nl local = {
		.nl_family = AF_NETLINK,
	};
	int ret;

	fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, protocol);
	if (fd == -1) {
		rte_errno = errno;
		return -rte_errno;
	}
	ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf_size, sizeof(int));
	if (ret == -1) {
		rte_errno = errno;
		goto error;
	}
	ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, sizeof(int));
	if (ret == -1) {
		rte_errno = errno;
		goto error;
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
	char buf[MLX5_RECV_BUF_SIZE];
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

		do {
			recv_bytes = recvmsg(nlsk_fd, &msg, 0);
			if (recv_bytes == -1) {
				rte_errno = errno;
				return -rte_errno;
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
					return -rte_errno;
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
			if (cb) {
				ret = cb(nh, arg);
				if (ret < 0)
					return ret;
			}
		}
	} while (multipart);
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
#ifndef NDEBUG
			char m[18];

			ether_format_addr(m, 18, RTA_DATA(attribute));
			DRV_LOG(DEBUG, "bridge MAC address %s", m);
#endif
			memcpy(&(*data->mac)[data->mac_n++],
			       RTA_DATA(attribute), ETHER_ADDR_LEN);
		}
	}
	return 0;
}

/**
 * Get bridge MAC addresses.
 *
 * @param dev
 *   Pointer to Ethernet device.
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
mlx5_nl_mac_addr_list(struct rte_eth_dev *dev, struct ether_addr (*mac)[],
		      int *mac_n)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int iface_idx = mlx5_ifindex(dev);
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
	int fd;
	int ret;
	uint32_t sn = priv->nl_sn++;

	if (priv->nl_socket_route == -1)
		return 0;
	fd = priv->nl_socket_route;
	ret = mlx5_nl_request(fd, &req.hdr, sn, &req.ifm,
			      sizeof(struct ifinfomsg));
	if (ret < 0)
		goto error;
	ret = mlx5_nl_recv(fd, sn, mlx5_nl_mac_addr_cb, &data);
	if (ret < 0)
		goto error;
	*mac_n = data.mac_n;
	return 0;
error:
	DRV_LOG(DEBUG, "port %u cannot retrieve MAC address list %s",
		dev->data->port_id, strerror(rte_errno));
	return -rte_errno;
}

/**
 * Modify the MAC address neighbour table with Netlink.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param mac
 *   MAC address to consider.
 * @param add
 *   1 to add the MAC address, 0 to remove the MAC address.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_nl_mac_addr_modify(struct rte_eth_dev *dev, struct ether_addr *mac,
			int add)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int iface_idx = mlx5_ifindex(dev);
	struct {
		struct nlmsghdr hdr;
		struct ndmsg ndm;
		struct rtattr rta;
		uint8_t buffer[ETHER_ADDR_LEN];
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
			.rta_len = RTA_LENGTH(ETHER_ADDR_LEN),
		},
	};
	int fd;
	int ret;
	uint32_t sn = priv->nl_sn++;

	if (priv->nl_socket_route == -1)
		return 0;
	fd = priv->nl_socket_route;
	memcpy(RTA_DATA(&req.rta), mac, ETHER_ADDR_LEN);
	req.hdr.nlmsg_len = NLMSG_ALIGN(req.hdr.nlmsg_len) +
		RTA_ALIGN(req.rta.rta_len);
	ret = mlx5_nl_send(fd, &req.hdr, sn);
	if (ret < 0)
		goto error;
	ret = mlx5_nl_recv(fd, sn, NULL, NULL);
	if (ret < 0)
		goto error;
	return 0;
error:
	DRV_LOG(DEBUG,
		"port %u cannot %s MAC address %02X:%02X:%02X:%02X:%02X:%02X"
		" %s",
		dev->data->port_id,
		add ? "add" : "remove",
		mac->addr_bytes[0], mac->addr_bytes[1],
		mac->addr_bytes[2], mac->addr_bytes[3],
		mac->addr_bytes[4], mac->addr_bytes[5],
		strerror(rte_errno));
	return -rte_errno;
}

/**
 * Add a MAC address.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param mac
 *   MAC address to register.
 * @param index
 *   MAC address index.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_nl_mac_addr_add(struct rte_eth_dev *dev, struct ether_addr *mac,
		     uint32_t index)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;

	ret = mlx5_nl_mac_addr_modify(dev, mac, 1);
	if (!ret)
		BITFIELD_SET(priv->mac_own, index);
	if (ret == -EEXIST)
		return 0;
	return ret;
}

/**
 * Remove a MAC address.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param mac
 *   MAC address to remove.
 * @param index
 *   MAC address index.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_nl_mac_addr_remove(struct rte_eth_dev *dev, struct ether_addr *mac,
			uint32_t index)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	BITFIELD_RESET(priv->mac_own, index);
	return mlx5_nl_mac_addr_modify(dev, mac, 0);
}

/**
 * Synchronize Netlink bridge table to the internal table.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_nl_mac_addr_sync(struct rte_eth_dev *dev)
{
	struct ether_addr macs[MLX5_MAX_MAC_ADDRESSES];
	int macs_n = 0;
	int i;
	int ret;

	ret = mlx5_nl_mac_addr_list(dev, &macs, &macs_n);
	if (ret)
		return;
	for (i = 0; i != macs_n; ++i) {
		int j;

		/* Verify the address is not in the array yet. */
		for (j = 0; j != MLX5_MAX_MAC_ADDRESSES; ++j)
			if (is_same_ether_addr(&macs[i],
					       &dev->data->mac_addrs[j]))
				break;
		if (j != MLX5_MAX_MAC_ADDRESSES)
			continue;
		/* Find the first entry available. */
		for (j = 0; j != MLX5_MAX_MAC_ADDRESSES; ++j) {
			if (is_zero_ether_addr(&dev->data->mac_addrs[j])) {
				dev->data->mac_addrs[j] = macs[i];
				break;
			}
		}
	}
}

/**
 * Flush all added MAC addresses.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_nl_mac_addr_flush(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int i;

	for (i = MLX5_MAX_MAC_ADDRESSES - 1; i >= 0; --i) {
		struct ether_addr *m = &dev->data->mac_addrs[i];

		if (BITFIELD_ISSET(priv->mac_own, i))
			mlx5_nl_mac_addr_remove(dev, m, i);
	}
}

/**
 * Enable promiscuous / all multicast mode through Netlink.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param flags
 *   IFF_PROMISC for promiscuous, IFF_ALLMULTI for allmulti.
 * @param enable
 *   Nonzero to enable, disable otherwise.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_nl_device_flags(struct rte_eth_dev *dev, uint32_t flags, int enable)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int iface_idx = mlx5_ifindex(dev);
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
	int fd;
	int ret;

	assert(!(flags & ~(IFF_PROMISC | IFF_ALLMULTI)));
	if (priv->nl_socket_route < 0)
		return 0;
	fd = priv->nl_socket_route;
	ret = mlx5_nl_send(fd, &req.hdr, priv->nl_sn++);
	if (ret < 0)
		return ret;
	return 0;
}

/**
 * Enable promiscuous mode through Netlink.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param enable
 *   Nonzero to enable, disable otherwise.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_nl_promisc(struct rte_eth_dev *dev, int enable)
{
	int ret = mlx5_nl_device_flags(dev, IFF_PROMISC, enable);

	if (ret)
		DRV_LOG(DEBUG,
			"port %u cannot %s promisc mode: Netlink error %s",
			dev->data->port_id, enable ? "enable" : "disable",
			strerror(rte_errno));
	return ret;
}

/**
 * Enable all multicast mode through Netlink.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param enable
 *   Nonzero to enable, disable otherwise.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_nl_allmulti(struct rte_eth_dev *dev, int enable)
{
	int ret = mlx5_nl_device_flags(dev, IFF_ALLMULTI, enable);

	if (ret)
		DRV_LOG(DEBUG,
			"port %u cannot %s allmulti mode: Netlink error %s",
			dev->data->port_id, enable ? "enable" : "disable",
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
	struct mlx5_nl_ifindex_data *data = arg;
	size_t off = NLMSG_HDRLEN;
	uint32_t ibindex = 0;
	uint32_t ifindex = 0;
	uint32_t portnum = 0;
	int found = 0;

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
			ibindex = *(uint32_t *)payload;
			break;
		case RDMA_NLDEV_ATTR_DEV_NAME:
			if (!strcmp(payload, data->name))
				found = 1;
			break;
		case RDMA_NLDEV_ATTR_NDEV_INDEX:
			ifindex = *(uint32_t *)payload;
			break;
		case RDMA_NLDEV_ATTR_PORT_INDEX:
			portnum = *(uint32_t *)payload;
			break;
		default:
			break;
		}
		off += NLA_ALIGN(na->nla_len);
	}
	if (found) {
		data->ibindex = ibindex;
		data->ifindex = ifindex;
		data->portnum = portnum;
	}
	return 0;
error:
	rte_errno = EINVAL;
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
 * @return
 *   A valid (nonzero) interface index on success, 0 otherwise and rte_errno
 *   is set.
 */
unsigned int
mlx5_nl_ifindex(int nl, const char *name, uint32_t pindex)
{
	uint32_t seq = random();
	struct mlx5_nl_ifindex_data data = {
		.name = name,
		.ibindex = 0, /* Determined during first pass. */
		.ifindex = 0, /* Determined during second pass. */
	};
	union {
		struct nlmsghdr nh;
		uint8_t buf[NLMSG_HDRLEN +
			    NLA_HDRLEN + NLA_ALIGN(sizeof(data.ibindex)) +
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
	int ret;

	ret = mlx5_nl_send(nl, &req.nh, seq);
	if (ret < 0)
		return 0;
	ret = mlx5_nl_recv(nl, seq, mlx5_nl_cmdget_cb, &data);
	if (ret < 0)
		return 0;
	if (!data.ibindex)
		goto error;
	++seq;
	req.nh.nlmsg_type = RDMA_NL_GET_TYPE(RDMA_NL_NLDEV,
					     RDMA_NLDEV_CMD_PORT_GET);
	req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(req.buf) - NLMSG_HDRLEN);
	na = (void *)((uintptr_t)req.buf + NLMSG_HDRLEN);
	na->nla_len = NLA_HDRLEN + sizeof(data.ibindex);
	na->nla_type = RDMA_NLDEV_ATTR_DEV_INDEX;
	memcpy((void *)((uintptr_t)na + NLA_HDRLEN),
	       &data.ibindex, sizeof(data.ibindex));
	na = (void *)((uintptr_t)na + NLA_ALIGN(na->nla_len));
	na->nla_len = NLA_HDRLEN + sizeof(pindex);
	na->nla_type = RDMA_NLDEV_ATTR_PORT_INDEX;
	memcpy((void *)((uintptr_t)na + NLA_HDRLEN),
	       &pindex, sizeof(pindex));
	ret = mlx5_nl_send(nl, &req.nh, seq);
	if (ret < 0)
		return 0;
	ret = mlx5_nl_recv(nl, seq, mlx5_nl_cmdget_cb, &data);
	if (ret < 0)
		return 0;
	if (!data.ifindex)
		goto error;
	return data.ifindex;
error:
	rte_errno = ENODEV;
	return 0;
}

/**
 * Get the number of physical ports of given IB device.
 *
 * @param nl
 *   Netlink socket of the RDMA kind (NETLINK_RDMA).
 * @param[in] name
 *   IB device name.
 *
 * @return
 *   A valid (nonzero) number of ports on success, 0 otherwise
 *   and rte_errno is set.
 */
unsigned int
mlx5_nl_portnum(int nl, const char *name)
{
	uint32_t seq = random();
	struct mlx5_nl_ifindex_data data = {
		.name = name,
		.ibindex = 0,
		.ifindex = 0,
		.portnum = 0,
	};
	struct nlmsghdr req = {
		.nlmsg_len = NLMSG_LENGTH(0),
		.nlmsg_type = RDMA_NL_GET_TYPE(RDMA_NL_NLDEV,
					       RDMA_NLDEV_CMD_GET),
		.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_DUMP,
	};
	int ret;

	ret = mlx5_nl_send(nl, &req, seq);
	if (ret < 0)
		return 0;
	ret = mlx5_nl_recv(nl, seq, mlx5_nl_cmdget_cb, &data);
	if (ret < 0)
		return 0;
	if (!data.ibindex) {
		rte_errno = ENODEV;
		return 0;
	}
	if (!data.portnum)
		rte_errno = EINVAL;
	return data.portnum;
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
			mlx5_translate_port_name((char *)payload, &info);
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
	assert(!(info.master && info.representor));
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
int
mlx5_nl_switch_info(int nl, unsigned int ifindex, struct mlx5_switch_info *info)
{
	uint32_t seq = random();
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
	int ret;

	ret = mlx5_nl_send(nl, &req.nh, seq);
	if (ret >= 0)
		ret = mlx5_nl_recv(nl, seq, mlx5_nl_switch_info_cb, info);
	if (info->master && info->representor) {
		DRV_LOG(ERR, "ifindex %u device is recognized as master"
			     " and as representor", ifindex);
		rte_errno = ENODEV;
		ret = -rte_errno;
	}
	return ret;
}
