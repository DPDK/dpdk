/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include <rte_ether.h>

#include "vhost_kernel_tap.h"
#include "../virtio_logs.h"
#include "../virtio_pci.h"

int
vhost_kernel_tap_set_offload(int fd, uint64_t features)
{
	unsigned int offload = 0;

	if (features & (1ULL << VIRTIO_NET_F_GUEST_CSUM)) {
		offload |= TUN_F_CSUM;
		if (features & (1ULL << VIRTIO_NET_F_GUEST_TSO4))
			offload |= TUN_F_TSO4;
		if (features & (1ULL << VIRTIO_NET_F_GUEST_TSO6))
			offload |= TUN_F_TSO6;
		if (features & ((1ULL << VIRTIO_NET_F_GUEST_TSO4) |
			(1ULL << VIRTIO_NET_F_GUEST_TSO6)) &&
			(features & (1ULL << VIRTIO_NET_F_GUEST_ECN)))
			offload |= TUN_F_TSO_ECN;
		if (features & (1ULL << VIRTIO_NET_F_GUEST_UFO))
			offload |= TUN_F_UFO;
	}

	/* Check if our kernel supports TUNSETOFFLOAD */
	if (ioctl(fd, TUNSETOFFLOAD, 0) != 0 && errno == EINVAL) {
		PMD_DRV_LOG(ERR, "Kernel doesn't support TUNSETOFFLOAD\n");
		return -ENOTSUP;
	}

	if (ioctl(fd, TUNSETOFFLOAD, offload) != 0) {
		offload &= ~TUN_F_UFO;
		if (ioctl(fd, TUNSETOFFLOAD, offload) != 0) {
			PMD_DRV_LOG(ERR, "TUNSETOFFLOAD ioctl() failed: %s\n",
				strerror(errno));
			return -1;
		}
	}

	return 0;
}

int
vhost_kernel_tap_set_queue(int fd, bool attach)
{
	struct ifreq ifr = {
		.ifr_flags = attach ? IFF_ATTACH_QUEUE : IFF_DETACH_QUEUE,
	};

	return ioctl(fd, TUNSETQUEUE, &ifr);
}

int
vhost_kernel_open_tap(char **p_ifname, int hdr_size, int req_mq,
			 const char *mac, uint64_t features)
{
	unsigned int tap_features;
	char *tap_name = NULL;
	int sndbuf = INT_MAX;
	struct ifreq ifr;
	int tapfd;
	int ret;

	/* TODO:
	 * 1. verify we can get/set vnet_hdr_len, tap_probe_vnet_hdr_len
	 * 2. get number of memory regions from vhost module parameter
	 * max_mem_regions, supported in newer version linux kernel
	 */
	tapfd = open(PATH_NET_TUN, O_RDWR);
	if (tapfd < 0) {
		PMD_DRV_LOG(ERR, "fail to open %s: %s",
			    PATH_NET_TUN, strerror(errno));
		return -1;
	}

	/* Construct ifr */
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

	if (ioctl(tapfd, TUNGETFEATURES, &tap_features) == -1) {
		PMD_DRV_LOG(ERR, "TUNGETFEATURES failed: %s", strerror(errno));
		goto error;
	}
	if (tap_features & IFF_ONE_QUEUE)
		ifr.ifr_flags |= IFF_ONE_QUEUE;

	/* Let tap instead of vhost-net handle vnet header, as the latter does
	 * not support offloading. And in this case, we should not set feature
	 * bit VHOST_NET_F_VIRTIO_NET_HDR.
	 */
	if (tap_features & IFF_VNET_HDR) {
		ifr.ifr_flags |= IFF_VNET_HDR;
	} else {
		PMD_DRV_LOG(ERR, "TAP does not support IFF_VNET_HDR");
		goto error;
	}

	if (req_mq)
		ifr.ifr_flags |= IFF_MULTI_QUEUE;

	if (*p_ifname)
		strncpy(ifr.ifr_name, *p_ifname, IFNAMSIZ - 1);
	else
		strncpy(ifr.ifr_name, "tap%d", IFNAMSIZ - 1);
	if (ioctl(tapfd, TUNSETIFF, (void *)&ifr) == -1) {
		PMD_DRV_LOG(ERR, "TUNSETIFF failed: %s", strerror(errno));
		goto error;
	}

	tap_name = strdup(ifr.ifr_name);
	if (!tap_name) {
		PMD_DRV_LOG(ERR, "strdup ifname failed: %s", strerror(errno));
		goto error;
	}

	if (fcntl(tapfd, F_SETFL, O_NONBLOCK) < 0) {
		PMD_DRV_LOG(ERR, "fcntl tapfd failed: %s", strerror(errno));
		goto error;
	}

	if (ioctl(tapfd, TUNSETVNETHDRSZ, &hdr_size) < 0) {
		PMD_DRV_LOG(ERR, "TUNSETVNETHDRSZ failed: %s", strerror(errno));
		goto error;
	}

	if (ioctl(tapfd, TUNSETSNDBUF, &sndbuf) < 0) {
		PMD_DRV_LOG(ERR, "TUNSETSNDBUF failed: %s", strerror(errno));
		goto error;
	}

	ret = vhost_kernel_tap_set_offload(tapfd, features);
	if (ret < 0 && ret != -ENOTSUP)
		goto error;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	memcpy(ifr.ifr_hwaddr.sa_data, mac, RTE_ETHER_ADDR_LEN);
	if (ioctl(tapfd, SIOCSIFHWADDR, (void *)&ifr) == -1) {
		PMD_DRV_LOG(ERR, "SIOCSIFHWADDR failed: %s", strerror(errno));
		goto error;
	}

	free(*p_ifname);
	*p_ifname = tap_name;

	return tapfd;
error:
	free(tap_name);
	close(tapfd);
	return -1;
}
