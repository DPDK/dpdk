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

#ifndef _VHOST_NET_CDEV_H_
#define _VHOST_NET_CDEV_H_
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/vhost.h>

#include <rte_log.h>

#include "rte_virtio_net.h"

extern struct vhost_net_device_ops const *ops;

/* Macros for printing using RTE_LOG */
#define RTE_LOGTYPE_VHOST_CONFIG RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_VHOST_DATA   RTE_LOGTYPE_USER1

#ifdef RTE_LIBRTE_VHOST_DEBUG
#define VHOST_MAX_PRINT_BUFF 6072
#define LOG_LEVEL RTE_LOG_DEBUG
#define LOG_DEBUG(log_type, fmt, args...) RTE_LOG(DEBUG, log_type, fmt, ##args)
#define PRINT_PACKET(device, addr, size, header) do { \
	char *pkt_addr = (char *)(addr); \
	unsigned int index; \
	char packet[VHOST_MAX_PRINT_BUFF]; \
	\
	if ((header)) \
		snprintf(packet, VHOST_MAX_PRINT_BUFF, "(%" PRIu64 ") Header size %d: ", (device->device_fh), (size)); \
	else \
		snprintf(packet, VHOST_MAX_PRINT_BUFF, "(%" PRIu64 ") Packet size %d: ", (device->device_fh), (size)); \
	for (index = 0; index < (size); index++) { \
		snprintf(packet + strnlen(packet, VHOST_MAX_PRINT_BUFF), VHOST_MAX_PRINT_BUFF - strnlen(packet, VHOST_MAX_PRINT_BUFF), \
			"%02hhx ", pkt_addr[index]); \
	} \
	snprintf(packet + strnlen(packet, VHOST_MAX_PRINT_BUFF), VHOST_MAX_PRINT_BUFF - strnlen(packet, VHOST_MAX_PRINT_BUFF), "\n"); \
	\
	LOG_DEBUG(VHOST_DATA, "%s", packet); \
} while (0)
#else
#define LOG_LEVEL RTE_LOG_INFO
#define LOG_DEBUG(log_type, fmt, args...) do {} while (0)
#define PRINT_PACKET(device, addr, size, header) do {} while (0)
#endif


/*
 * Structure used to identify device context.
 */
struct vhost_device_ctx {
	pid_t		pid;	/* PID of process calling the IOCTL. */
	uint64_t	fh;	/* Populated with fi->fh to track the device index. */
};

/*
 * Structure contains function pointers to be defined in virtio-net.c. These
 * functions are called in CUSE context and are used to configure devices.
 */
struct vhost_net_device_ops {
	int (*new_device)(struct vhost_device_ctx);
	void (*destroy_device)(struct vhost_device_ctx);

	void (*set_ifname)(struct vhost_device_ctx,
		const char *if_name, unsigned int if_len);

	int (*get_features)(struct vhost_device_ctx, uint64_t *);
	int (*set_features)(struct vhost_device_ctx, uint64_t *);

	int (*set_vring_num)(struct vhost_device_ctx, struct vhost_vring_state *);
	int (*set_vring_addr)(struct vhost_device_ctx, struct vhost_vring_addr *);
	int (*set_vring_base)(struct vhost_device_ctx, struct vhost_vring_state *);
	int (*get_vring_base)(struct vhost_device_ctx, uint32_t, struct vhost_vring_state *);

	int (*set_vring_kick)(struct vhost_device_ctx, struct vhost_vring_file *);
	int (*set_vring_call)(struct vhost_device_ctx, struct vhost_vring_file *);

	int (*set_backend)(struct vhost_device_ctx, struct vhost_vring_file *);

	int (*set_owner)(struct vhost_device_ctx);
	int (*reset_owner)(struct vhost_device_ctx);
};


struct vhost_net_device_ops const *get_virtio_net_callbacks(void);
#endif /* _VHOST_NET_CDEV_H_ */
