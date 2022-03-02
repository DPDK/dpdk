/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

#ifndef _VHOST_KERNEL_TAP_H
#define _VHOST_KERNEL_TAP_H

#include <stdbool.h>
#include <sys/ioctl.h>

/* TUN ioctls */
#define TUNSETIFF     _IOW('T', 202, int)
#define TUNGETFEATURES _IOR('T', 207, unsigned int)
#define TUNSETOFFLOAD  _IOW('T', 208, unsigned int)
#define TUNGETIFF      _IOR('T', 210, unsigned int)
#define TUNSETSNDBUF   _IOW('T', 212, int)
#define TUNGETVNETHDRSZ _IOR('T', 215, int)
#define TUNSETVNETHDRSZ _IOW('T', 216, int)

/* TUNSETIFF ifr flags */
#define IFF_TAP          0x0002
#define IFF_NO_PI        0x1000
#define IFF_VNET_HDR     0x4000
#define IFF_MULTI_QUEUE  0x0100
#define IFF_NAPI         0x0010

/* Features for GSO (TUNSETOFFLOAD). */
#define TUN_F_CSUM	0x01	/* You can hand me unchecksummed packets. */
#define TUN_F_TSO4	0x02	/* I can handle TSO for IPv4 packets */
#define TUN_F_TSO6	0x04	/* I can handle TSO for IPv6 packets */
#define TUN_F_TSO_ECN	0x08	/* I can handle TSO with ECN bits. */
#define TUN_F_UFO	0x10	/* I can handle UFO packets */

/* Constants */
#define PATH_NET_TUN	"/dev/net/tun"

int vhost_kernel_tap_setup(int tapfd, int hdr_size, uint64_t features);

int tap_support_features(unsigned int *tap_features);
int tap_open(const char *ifname, unsigned int r_flags, bool multi_queue);
int tap_get_name(int tapfd, char **ifname);
int tap_get_flags(int tapfd, unsigned int *tap_flags);
int tap_set_mac(int tapfd, uint8_t *mac);

#endif
