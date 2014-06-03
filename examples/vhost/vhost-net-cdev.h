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

#include <linux/vhost.h>

struct vhost_memory;
struct vhost_vring_state;
struct vhost_vring_addr;
struct vhost_vring_file;

/*
 * Structure used to identify device context.
 */
struct vhost_device_ctx
{
	pid_t		pid;	/* PID of process calling the IOCTL. */
	uint64_t 	fh;		/* Populated with fi->fh to track the device index. */
};

/*
 * Structure contains function pointers to be defined in virtio-net.c. These
 * functions are called in CUSE context and are used to configure devices.
 */
struct vhost_net_device_ops {
	int (* new_device) 		(struct vhost_device_ctx);
	void (* destroy_device) (struct vhost_device_ctx);

	int (* get_features) 	(struct vhost_device_ctx, uint64_t *);
	int (* set_features) 	(struct vhost_device_ctx, uint64_t *);

	int (* set_mem_table) 	(struct vhost_device_ctx, const void *, uint32_t);

	int (* set_vring_num) 	(struct vhost_device_ctx, struct vhost_vring_state *);
	int (* set_vring_addr) 	(struct vhost_device_ctx, struct vhost_vring_addr *);
	int (* set_vring_base) 	(struct vhost_device_ctx, struct vhost_vring_state *);
	int (* get_vring_base) 	(struct vhost_device_ctx, uint32_t, struct vhost_vring_state *);

	int (* set_vring_kick) 	(struct vhost_device_ctx, struct vhost_vring_file *);
	int (* set_vring_call) 	(struct vhost_device_ctx, struct vhost_vring_file *);

	int (* set_backend) 	(struct vhost_device_ctx, struct vhost_vring_file *);

	int (* set_owner) 		(struct vhost_device_ctx);
	int (* reset_owner) 	(struct vhost_device_ctx);
};

int register_cuse_device(const char *base_name, int index, struct vhost_net_device_ops const * const);
int start_cuse_session_loop(void);

#endif /* _VHOST_NET_CDEV_H_ */
