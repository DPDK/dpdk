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
#include "../virtio.h"


int
tap_support_features(unsigned int *tap_features)
{
	int tapfd;

	tapfd = open(PATH_NET_TUN, O_RDWR);
	if (tapfd < 0) {
		PMD_DRV_LOG(ERR, "fail to open %s: %s",
			    PATH_NET_TUN, strerror(errno));
		return -1;
	}

	if (ioctl(tapfd, TUNGETFEATURES, tap_features) == -1) {
		PMD_DRV_LOG(ERR, "TUNGETFEATURES failed: %s", strerror(errno));
		close(tapfd);
		return -1;
	}

	close(tapfd);
	return 0;
}

int
tap_open(const char *ifname, bool multi_queue)
{
	struct ifreq ifr;
	int tapfd;

	tapfd = open(PATH_NET_TUN, O_RDWR);
	if (tapfd < 0) {
		PMD_DRV_LOG(ERR, "fail to open %s: %s", PATH_NET_TUN, strerror(errno));
		return -1;
	}
	if (fcntl(tapfd, F_SETFL, O_NONBLOCK) < 0) {
		PMD_DRV_LOG(ERR, "fcntl tapfd failed: %s", strerror(errno));
		close(tapfd);
		return -1;
	}

retry_mono_q:
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI | IFF_VNET_HDR;
	if (multi_queue)
		ifr.ifr_flags |= IFF_MULTI_QUEUE;
	if (ioctl(tapfd, TUNSETIFF, (void *)&ifr) == -1) {
		if (multi_queue) {
			PMD_DRV_LOG(DEBUG,
				"TUNSETIFF failed (will retry without IFF_MULTI_QUEUE): %s",
				strerror(errno));
			multi_queue = false;
			goto retry_mono_q;
		}

		PMD_DRV_LOG(ERR, "TUNSETIFF failed: %s", strerror(errno));
		close(tapfd);
		tapfd = -1;
	}
	return tapfd;
}

int
tap_get_name(int tapfd, char **name)
{
	struct ifreq ifr;
	int ret;

	memset(&ifr, 0, sizeof(ifr));
	if (ioctl(tapfd, TUNGETIFF, (void *)&ifr) == -1) {
		PMD_DRV_LOG(ERR, "TUNGETIFF failed: %s", strerror(errno));
		return -1;
	}
	ret = asprintf(name, "%s", ifr.ifr_name);
	if (ret != -1)
		ret = 0;
	return ret;
}

int
tap_get_flags(int tapfd, unsigned int *tap_flags)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	if (ioctl(tapfd, TUNGETIFF, (void *)&ifr) == -1) {
		PMD_DRV_LOG(ERR, "TUNGETIFF failed: %s", strerror(errno));
		return -1;
	}
	*tap_flags = ifr.ifr_flags;
	return 0;
}

int
tap_set_mac(int tapfd, uint8_t *mac)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	memcpy(ifr.ifr_hwaddr.sa_data, mac, RTE_ETHER_ADDR_LEN);
	if (ioctl(tapfd, SIOCSIFHWADDR, (void *)&ifr) == -1) {
		PMD_DRV_LOG(ERR, "SIOCSIFHWADDR failed: %s", strerror(errno));
		return -1;
	}
	return 0;
}

static int
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
		PMD_DRV_LOG(ERR, "Kernel doesn't support TUNSETOFFLOAD");
		return -ENOTSUP;
	}

	if (ioctl(fd, TUNSETOFFLOAD, offload) != 0) {
		offload &= ~TUN_F_UFO;
		if (ioctl(fd, TUNSETOFFLOAD, offload) != 0) {
			PMD_DRV_LOG(ERR, "TUNSETOFFLOAD ioctl() failed: %s",
				strerror(errno));
			return -1;
		}
	}

	return 0;
}

int
vhost_kernel_tap_setup(int tapfd, int hdr_size, uint64_t features)
{
	int sndbuf = INT_MAX;
	int ret;

	/* TODO:
	 * 1. verify we can get/set vnet_hdr_len, tap_probe_vnet_hdr_len
	 * 2. get number of memory regions from vhost module parameter
	 * max_mem_regions, supported in newer version linux kernel
	 */
	if (ioctl(tapfd, TUNSETVNETHDRSZ, &hdr_size) < 0) {
		PMD_DRV_LOG(ERR, "TUNSETVNETHDRSZ failed: %s", strerror(errno));
		return -1;
	}

	if (ioctl(tapfd, TUNSETSNDBUF, &sndbuf) < 0) {
		PMD_DRV_LOG(ERR, "TUNSETSNDBUF failed: %s", strerror(errno));
		return -1;
	}

	ret = vhost_kernel_tap_set_offload(tapfd, features);
	if (ret == -ENOTSUP)
		ret = 0;
	return ret;
}
