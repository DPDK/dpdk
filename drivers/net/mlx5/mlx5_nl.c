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
#include <stdalign.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_hypervisor.h>

#include "mlx5.h"
#include "mlx5_utils.h"

/* Size of the buffer to receive kernel messages */
#define MLX5_NL_BUF_SIZE (32 * 1024)
/* Send buffer size for the Netlink socket */
#define MLX5_SEND_BUF_SIZE 32768
/* Receive buffer size for the Netlink socket */
#define MLX5_RECV_BUF_SIZE 32768

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
	struct rte_ether_addr (*mac)[];
	/**< MAC address handled by the device. */
	int mac_n; /**< Number of addresses in the array. */
};

#define MLX5_NL_CMD_GET_IB_NAME (1 << 0)
#define MLX5_NL_CMD_GET_IB_INDEX (1 << 1)
#define MLX5_NL_CMD_GET_NET_INDEX (1 << 2)
#define MLX5_NL_CMD_GET_PORT_INDEX (1 << 3)

/** Data structure used by mlx5_nl_cmdget_cb(). */
struct mlx5_nl_ifindex_data {
	const char *name; /**< IB device name (in). */
	uint32_t flags; /**< found attribute flags (out). */
	uint32_t ibindex; /**< IB device index (out). */
	uint32_t ifindex; /**< Network interface index (out). */
	uint32_t portnum; /**< IB device max port number (out). */
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

			rte_ether_format_addr(m, 18, RTA_DATA(attribute));
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
mlx5_nl_mac_addr_list(struct rte_eth_dev *dev, struct rte_ether_addr (*mac)[],
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
mlx5_nl_mac_addr_modify(struct rte_eth_dev *dev, struct rte_ether_addr *mac,
			int add)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int iface_idx = mlx5_ifindex(dev);
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
	int fd;
	int ret;
	uint32_t sn = priv->nl_sn++;

	if (priv->nl_socket_route == -1)
		return 0;
	fd = priv->nl_socket_route;
	memcpy(RTA_DATA(&req.rta), mac, RTE_ETHER_ADDR_LEN);
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
 * Modify the VF MAC address neighbour table with Netlink.
 *
 * @param dev
 *    Pointer to Ethernet device.
 * @param mac
 *    MAC address to consider.
 * @param vf_index
 *    VF index.
 *
 * @return
 *    0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_nl_vf_mac_addr_modify(struct rte_eth_dev *dev,
			   struct rte_ether_addr *mac, int vf_index)
{
	int fd, ret;
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int iface_idx = mlx5_ifindex(dev);
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
	uint32_t sn = priv->nl_sn++;
	struct ifla_vf_mac ivm = {
		.vf = vf_index,
	};

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

	fd = priv->nl_socket_route;
	if (fd < 0)
		return -1;
	ret = mlx5_nl_send(fd, &req.hdr, sn);
	if (ret < 0)
		goto error;
	ret = mlx5_nl_recv(fd, sn, NULL, NULL);
	if (ret < 0)
		goto error;
	return 0;
error:
	DRV_LOG(ERR,
		"representor %u cannot set VF MAC address "
		"%02X:%02X:%02X:%02X:%02X:%02X : %s",
		vf_index,
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
mlx5_nl_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac,
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
mlx5_nl_mac_addr_remove(struct rte_eth_dev *dev, struct rte_ether_addr *mac,
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
	struct rte_ether_addr macs[MLX5_MAX_MAC_ADDRESSES];
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
			if (rte_is_same_ether_addr(&macs[i],
					       &dev->data->mac_addrs[j]))
				break;
		if (j != MLX5_MAX_MAC_ADDRESSES)
			continue;
		/* Find the first entry available. */
		for (j = 0; j != MLX5_MAX_MAC_ADDRESSES; ++j) {
			if (rte_is_zero_ether_addr(&dev->data->mac_addrs[j])) {
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
		struct rte_ether_addr *m = &dev->data->mac_addrs[i];

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
	struct mlx5_nl_ifindex_data local = {
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
		.flags = 0,
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
	if (!(data.flags & MLX5_NL_CMD_GET_IB_NAME) ||
	    !(data.flags & MLX5_NL_CMD_GET_IB_INDEX))
		goto error;
	data.flags = 0;
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
	if (!(data.flags & MLX5_NL_CMD_GET_IB_NAME) ||
	    !(data.flags & MLX5_NL_CMD_GET_IB_INDEX) ||
	    !(data.flags & MLX5_NL_CMD_GET_NET_INDEX) ||
	    !data.ifindex)
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
	int ret;

	ret = mlx5_nl_send(nl, &req, seq);
	if (ret < 0)
		return 0;
	ret = mlx5_nl_recv(nl, seq, mlx5_nl_cmdget_cb, &data);
	if (ret < 0)
		return 0;
	if (!(data.flags & MLX5_NL_CMD_GET_IB_NAME) ||
	    !(data.flags & MLX5_NL_CMD_GET_IB_INDEX) ||
	    !(data.flags & MLX5_NL_CMD_GET_PORT_INDEX)) {
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

/*
 * Delete VLAN network device by ifindex.
 *
 * @param[in] tcf
 *   Context object initialized by mlx5_vlan_vmwa_init().
 * @param[in] ifindex
 *   Interface index of network device to delete.
 */
static void
mlx5_vlan_vmwa_delete(struct mlx5_vlan_vmwa_context *vmwa,
		      uint32_t ifindex)
{
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
		++vmwa->nl_sn;
		if (!vmwa->nl_sn)
			++vmwa->nl_sn;
		ret = mlx5_nl_send(vmwa->nl_socket, &req.nh, vmwa->nl_sn);
		if (ret >= 0)
			ret = mlx5_nl_recv(vmwa->nl_socket,
					   vmwa->nl_sn,
					   NULL, NULL);
		if (ret < 0)
			DRV_LOG(WARNING, "netlink: error deleting"
					 " VLAN WA ifindex %u, %d",
					 ifindex, ret);
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
	nla->nla_len = NLMSG_ALIGN(sizeof(struct nlattr) + alen);
	nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) + nla->nla_len;

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
 *   Context object initialized by mlx5_vlan_vmwa_init().
 * @param[in] ifindex
 *   Base network interface index.
 * @param[in] tag
 *   VLAN tag for VLAN network device to create.
 */
static uint32_t
mlx5_vlan_vmwa_create(struct mlx5_vlan_vmwa_context *vmwa,
		      uint32_t ifindex,
		      uint16_t tag)
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
	int ret;

	memset(buf, 0, sizeof(buf));
	++vmwa->nl_sn;
	if (!vmwa->nl_sn)
		++vmwa->nl_sn;
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
	assert(sizeof(buf) >= nlh->nlmsg_len);
	ret = mlx5_nl_send(vmwa->nl_socket, nlh, vmwa->nl_sn);
	if (ret >= 0)
		ret = mlx5_nl_recv(vmwa->nl_socket, vmwa->nl_sn, NULL, NULL);
	if (ret < 0) {
		DRV_LOG(WARNING,
			"netlink: VLAN %s create failure (%d)",
			name, ret);
	}
	// Try to get ifindex of created or pre-existing device.
	ret = if_nametoindex(name);
	if (!ret) {
		DRV_LOG(WARNING,
			"VLAN %s failed to get index (%d)",
			name, errno);
		return 0;
	}
	return ret;
}

/*
 * Release VLAN network device, created for VM workaround.
 *
 * @param[in] dev
 *   Ethernet device object, Netlink context provider.
 * @param[in] vlan
 *   Object representing the network device to release.
 */
void mlx5_vlan_vmwa_release(struct rte_eth_dev *dev,
			    struct mlx5_vf_vlan *vlan)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_vlan_vmwa_context *vmwa = priv->vmwa_context;
	struct mlx5_vlan_dev *vlan_dev = &vmwa->vlan_dev[0];

	assert(vlan->created);
	assert(priv->vmwa_context);
	if (!vlan->created || !vmwa)
		return;
	vlan->created = 0;
	assert(vlan_dev[vlan->tag].refcnt);
	if (--vlan_dev[vlan->tag].refcnt == 0 &&
	    vlan_dev[vlan->tag].ifindex) {
		mlx5_vlan_vmwa_delete(vmwa, vlan_dev[vlan->tag].ifindex);
		vlan_dev[vlan->tag].ifindex = 0;
	}
}

/**
 * Acquire VLAN interface with specified tag for VM workaround.
 *
 * @param[in] dev
 *   Ethernet device object, Netlink context provider.
 * @param[in] vlan
 *   Object representing the network device to acquire.
 */
void mlx5_vlan_vmwa_acquire(struct rte_eth_dev *dev,
			    struct mlx5_vf_vlan *vlan)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_vlan_vmwa_context *vmwa = priv->vmwa_context;
	struct mlx5_vlan_dev *vlan_dev = &vmwa->vlan_dev[0];

	assert(!vlan->created);
	assert(priv->vmwa_context);
	if (vlan->created || !vmwa)
		return;
	if (vlan_dev[vlan->tag].refcnt == 0) {
		assert(!vlan_dev[vlan->tag].ifindex);
		vlan_dev[vlan->tag].ifindex =
			mlx5_vlan_vmwa_create(vmwa,
					      vmwa->vf_ifindex,
					      vlan->tag);
	}
	if (vlan_dev[vlan->tag].ifindex) {
		vlan_dev[vlan->tag].refcnt++;
		vlan->created = 1;
	}
}

/*
 * Create per ethernet device VLAN VM workaround context
 */
struct mlx5_vlan_vmwa_context *
mlx5_vlan_vmwa_init(struct rte_eth_dev *dev,
		    uint32_t ifindex)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *config = &priv->config;
	struct mlx5_vlan_vmwa_context *vmwa;
	enum rte_hypervisor hv_type;

	/* Do not engage workaround over PF. */
	if (!config->vf)
		return NULL;
	/* Check whether there is desired virtual environment */
	hv_type = rte_hypervisor_get();
	switch (hv_type) {
	case RTE_HYPERVISOR_UNKNOWN:
	case RTE_HYPERVISOR_VMWARE:
		/*
		 * The "white list" of configurations
		 * to engage the workaround.
		 */
		break;
	default:
		/*
		 * The configuration is not found in the "white list".
		 * We should not engage the VLAN workaround.
		 */
		return NULL;
	}
	vmwa = rte_zmalloc(__func__, sizeof(*vmwa), sizeof(uint32_t));
	if (!vmwa) {
		DRV_LOG(WARNING,
			"Can not allocate memory"
			" for VLAN workaround context");
		return NULL;
	}
	vmwa->nl_socket = mlx5_nl_init(NETLINK_ROUTE);
	if (vmwa->nl_socket < 0) {
		DRV_LOG(WARNING,
			"Can not create Netlink socket"
			" for VLAN workaround context");
		rte_free(vmwa);
		return NULL;
	}
	vmwa->nl_sn = random();
	vmwa->vf_ifindex = ifindex;
	vmwa->dev = dev;
	/* Cleanup for existing VLAN devices. */
	return vmwa;
}

/*
 * Destroy per ethernet device VLAN VM workaround context
 */
void mlx5_vlan_vmwa_exit(struct mlx5_vlan_vmwa_context *vmwa)
{
	unsigned int i;

	/* Delete all remaining VLAN devices. */
	for (i = 0; i < RTE_DIM(vmwa->vlan_dev); i++) {
		if (vmwa->vlan_dev[i].ifindex)
			mlx5_vlan_vmwa_delete(vmwa, vmwa->vlan_dev[i].ifindex);
	}
	if (vmwa->nl_socket >= 0)
		close(vmwa->nl_socket);
	rte_free(vmwa);
}
