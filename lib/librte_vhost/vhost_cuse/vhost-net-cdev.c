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

#include <errno.h>
#include <fuse/cuse_lowlevel.h>
#include <linux/limits.h>
#include <linux/vhost.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#include <rte_virtio_net.h>

#include "virtio-net-cdev.h"
#include "vhost-net.h"
#include "eventfd_copy.h"

#define FUSE_OPT_DUMMY "\0\0"
#define FUSE_OPT_FORE  "-f\0\0"
#define FUSE_OPT_NOMULTI "-s\0\0"

static const uint32_t default_major = 231;
static const uint32_t default_minor = 1;
static const char cuse_device_name[] = "/dev/cuse";
static const char default_cdev[] = "vhost-net";

static struct fuse_session *session;

/*
 * Returns vhost_cuse_device_ctx from given fuse_req_t. The
 * index is populated later when the device is added to the
 * device linked list.
 */
static struct vhost_cuse_device_ctx
fuse_req_to_vhost_ctx(fuse_req_t req, struct fuse_file_info *fi)
{
	struct vhost_cuse_device_ctx ctx;
	struct fuse_ctx const *const req_ctx = fuse_req_ctx(req);

	ctx.pid = req_ctx->pid;
	ctx.vid = (int)fi->fh;

	return ctx;
}

/*
 * When the device is created in QEMU it gets initialised here and
 * added to the device linked list.
 */
static void
vhost_net_open(fuse_req_t req, struct fuse_file_info *fi)
{
	int vid = 0;

	vid = vhost_new_device();
	if (vid == -1) {
		fuse_reply_err(req, EPERM);
		return;
	}

	fi->fh = vid;

	RTE_LOG(INFO, VHOST_CONFIG,
		"(%d) device configuration started\n", vid);
	fuse_reply_open(req, fi);
}

/*
 * When QEMU is shutdown or killed the device gets released.
 */
static void
vhost_net_release(fuse_req_t req, struct fuse_file_info *fi)
{
	int err = 0;
	struct vhost_cuse_device_ctx ctx = fuse_req_to_vhost_ctx(req, fi);

	vhost_destroy_device(ctx.vid);
	RTE_LOG(INFO, VHOST_CONFIG, "(%d) device released\n", ctx.vid);
	fuse_reply_err(req, err);
}

/*
 * Boilerplate code for CUSE IOCTL
 * Implicit arguments: vid, req, result.
 */
#define VHOST_IOCTL(func) do {	\
	result = (func)(vid);	\
	fuse_reply_ioctl(req, result, NULL, 0);	\
} while (0)

/*
 * Boilerplate IOCTL RETRY
 * Implicit arguments: req.
 */
#define VHOST_IOCTL_RETRY(size_r, size_w) do {	\
	struct iovec iov_r = { arg, (size_r) };	\
	struct iovec iov_w = { arg, (size_w) };	\
	fuse_reply_ioctl_retry(req, &iov_r,	\
		(size_r) ? 1 : 0, &iov_w, (size_w) ? 1 : 0);\
} while (0)

/*
 * Boilerplate code for CUSE Read IOCTL
 * Implicit arguments: vid, req, result, in_bufsz, in_buf.
 */
#define VHOST_IOCTL_R(type, var, func) do {	\
	if (!in_bufsz) {	\
		VHOST_IOCTL_RETRY(sizeof(type), 0);\
	} else {	\
		(var) = *(const type*)in_buf;	\
		result = func(vid, &(var));	\
		fuse_reply_ioctl(req, result, NULL, 0);\
	}	\
} while (0)

/*
 * Boilerplate code for CUSE Write IOCTL
 * Implicit arguments: vid, req, result, out_bufsz.
 */
#define VHOST_IOCTL_W(type, var, func) do {	\
	if (!out_bufsz) {	\
		VHOST_IOCTL_RETRY(0, sizeof(type));\
	} else {	\
		result = (func)(vid, &(var));\
		fuse_reply_ioctl(req, result, &(var), sizeof(type));\
	} \
} while (0)

/*
 * Boilerplate code for CUSE Read/Write IOCTL
 * Implicit arguments: vid, req, result, in_bufsz, in_buf.
 */
#define VHOST_IOCTL_RW(type1, var1, type2, var2, func) do {	\
	if (!in_bufsz) {	\
		VHOST_IOCTL_RETRY(sizeof(type1), sizeof(type2));\
	} else {	\
		(var1) = *(const type1*) (in_buf);	\
		result = (func)(vid, (var1), &(var2));	\
		fuse_reply_ioctl(req, result, &(var2), sizeof(type2));\
	}	\
} while (0)

/*
 * The IOCTLs are handled using CUSE/FUSE in userspace. Depending on the type
 * of IOCTL a buffer is requested to read or to write. This request is handled
 * by FUSE and the buffer is then given to CUSE.
 */
static void
vhost_net_ioctl(fuse_req_t req, int cmd, void *arg,
		struct fuse_file_info *fi, __rte_unused unsigned flags,
		const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
	struct vhost_cuse_device_ctx ctx = fuse_req_to_vhost_ctx(req, fi);
	struct vhost_vring_file file;
	struct vhost_vring_state state;
	struct vhost_vring_addr addr;
	uint64_t features;
	uint32_t index;
	int result = 0;
	int vid = ctx.vid;

	switch (cmd) {
	case VHOST_NET_SET_BACKEND:
		LOG_DEBUG(VHOST_CONFIG,
			"(%d) IOCTL: VHOST_NET_SET_BACKEND\n", ctx.vid);
		if (!in_buf) {
			VHOST_IOCTL_RETRY(sizeof(file), 0);
			break;
		}
		file = *(const struct vhost_vring_file *)in_buf;
		result = cuse_set_backend(ctx, &file);
		fuse_reply_ioctl(req, result, NULL, 0);
		break;

	case VHOST_GET_FEATURES:
		LOG_DEBUG(VHOST_CONFIG,
			"(%d) IOCTL: VHOST_GET_FEATURES\n", vid);
		VHOST_IOCTL_W(uint64_t, features, vhost_get_features);
		break;

	case VHOST_SET_FEATURES:
		LOG_DEBUG(VHOST_CONFIG,
			"(%d) IOCTL: VHOST_SET_FEATURES\n", vid);
		VHOST_IOCTL_R(uint64_t, features, vhost_set_features);
		break;

	case VHOST_RESET_OWNER:
		LOG_DEBUG(VHOST_CONFIG,
			"(%d) IOCTL: VHOST_RESET_OWNER\n", vid);
		VHOST_IOCTL(vhost_reset_owner);
		break;

	case VHOST_SET_OWNER:
		LOG_DEBUG(VHOST_CONFIG,
			"(%d) IOCTL: VHOST_SET_OWNER\n", vid);
		VHOST_IOCTL(vhost_set_owner);
		break;

	case VHOST_SET_MEM_TABLE:
		/*TODO fix race condition.*/
		LOG_DEBUG(VHOST_CONFIG,
			"(%d) IOCTL: VHOST_SET_MEM_TABLE\n", vid);
		static struct vhost_memory mem_temp;

		switch (in_bufsz) {
		case 0:
			VHOST_IOCTL_RETRY(sizeof(struct vhost_memory), 0);
			break;

		case sizeof(struct vhost_memory):
			mem_temp = *(const struct vhost_memory *) in_buf;

			if (mem_temp.nregions > 0) {
				VHOST_IOCTL_RETRY(sizeof(struct vhost_memory) +
					(sizeof(struct vhost_memory_region) *
						mem_temp.nregions), 0);
			} else {
				result = -1;
				fuse_reply_ioctl(req, result, NULL, 0);
			}
			break;

		default:
			result = cuse_set_mem_table(ctx, in_buf,
				mem_temp.nregions);
			if (result)
				fuse_reply_err(req, EINVAL);
			else
				fuse_reply_ioctl(req, result, NULL, 0);
		}
		break;

	case VHOST_SET_VRING_NUM:
		LOG_DEBUG(VHOST_CONFIG,
			"(%d) IOCTL: VHOST_SET_VRING_NUM\n", vid);
		VHOST_IOCTL_R(struct vhost_vring_state, state,
			vhost_set_vring_num);
		break;

	case VHOST_SET_VRING_BASE:
		LOG_DEBUG(VHOST_CONFIG,
			"(%d) IOCTL: VHOST_SET_VRING_BASE\n", vid);
		VHOST_IOCTL_R(struct vhost_vring_state, state,
			vhost_set_vring_base);
		break;

	case VHOST_GET_VRING_BASE:
		LOG_DEBUG(VHOST_CONFIG,
			"(%d) IOCTL: VHOST_GET_VRING_BASE\n", vid);
		VHOST_IOCTL_RW(uint32_t, index,
			struct vhost_vring_state, state, vhost_get_vring_base);
		break;

	case VHOST_SET_VRING_ADDR:
		LOG_DEBUG(VHOST_CONFIG,
			"(%d) IOCTL: VHOST_SET_VRING_ADDR\n", vid);
		VHOST_IOCTL_R(struct vhost_vring_addr, addr,
			vhost_set_vring_addr);
		break;

	case VHOST_SET_VRING_KICK:
	case VHOST_SET_VRING_CALL:
		if (cmd == VHOST_SET_VRING_KICK)
			LOG_DEBUG(VHOST_CONFIG,
				"(%d) IOCTL: VHOST_SET_VRING_KICK\n", vid);
		else
			LOG_DEBUG(VHOST_CONFIG,
				"(%d) IOCTL: VHOST_SET_VRING_CALL\n", vid);
		if (!in_buf)
			VHOST_IOCTL_RETRY(sizeof(struct vhost_vring_file), 0);
		else {
			int fd;
			file = *(const struct vhost_vring_file *)in_buf;
			LOG_DEBUG(VHOST_CONFIG,
				"idx:%d fd:%d\n", file.index, file.fd);
			fd = eventfd_copy(file.fd, ctx.pid);
			if (fd < 0) {
				fuse_reply_ioctl(req, -1, NULL, 0);
				result = -1;
				break;
			}
			file.fd = fd;
			if (cmd == VHOST_SET_VRING_KICK) {
				result = vhost_set_vring_kick(vid, &file);
				fuse_reply_ioctl(req, result, NULL, 0);
			} else {
				result = vhost_set_vring_call(vid, &file);
				fuse_reply_ioctl(req, result, NULL, 0);
			}
		}
		break;

	default:
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%d) IOCTL: DOESN NOT EXIST\n", vid);
		result = -1;
		fuse_reply_ioctl(req, result, NULL, 0);
	}

	if (result < 0)
		LOG_DEBUG(VHOST_CONFIG,
			"(%d) IOCTL: FAIL\n", vid);
	else
		LOG_DEBUG(VHOST_CONFIG,
			"(%d) IOCTL: SUCCESS\n", vid);
}

/*
 * Structure handling open, release and ioctl function pointers is populated.
 */
static const struct cuse_lowlevel_ops vhost_net_ops = {
	.open		= vhost_net_open,
	.release	= vhost_net_release,
	.ioctl		= vhost_net_ioctl,
};

/*
 * cuse_info is populated and used to register the cuse device.
 * vhost_net_device_ops are also passed when the device is registered in app.
 */
int
rte_vhost_driver_register(const char *dev_name, uint64_t flags)
{
	struct cuse_info cuse_info;
	char device_name[PATH_MAX] = "";
	char char_device_name[PATH_MAX] = "";
	const char *device_argv[] = { device_name };

	char fuse_opt_dummy[] = FUSE_OPT_DUMMY;
	char fuse_opt_fore[] = FUSE_OPT_FORE;
	char fuse_opt_nomulti[] = FUSE_OPT_NOMULTI;
	char *fuse_argv[] = {fuse_opt_dummy, fuse_opt_fore, fuse_opt_nomulti};

	if (flags) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"vhost-cuse does not support any flags so far\n");
		return -1;
	}

	if (access(cuse_device_name, R_OK | W_OK) < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"char device %s can't be accessed, maybe not exist\n",
			cuse_device_name);
		return -1;
	}

	if (eventfd_init() < 0)
		return -1;

	/*
	 * The device name is created. This is passed to QEMU so that it can
	 * register the device with our application.
	 */
	snprintf(device_name, PATH_MAX, "DEVNAME=%s", dev_name);
	snprintf(char_device_name, PATH_MAX, "/dev/%s", dev_name);

	/* Check if device already exists. */
	if (access(char_device_name, F_OK) != -1) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"char device %s already exists\n", char_device_name);
		return -1;
	}

	memset(&cuse_info, 0, sizeof(cuse_info));
	cuse_info.dev_major = default_major;
	cuse_info.dev_minor = default_minor;
	cuse_info.dev_info_argc = 1;
	cuse_info.dev_info_argv = device_argv;
	cuse_info.flags = CUSE_UNRESTRICTED_IOCTL;

	session = cuse_lowlevel_setup(3, fuse_argv,
			&cuse_info, &vhost_net_ops, 0, NULL);
	if (session == NULL)
		return -1;

	return 0;
}

/**
 * An empty function for unregister
 */
int
rte_vhost_driver_unregister(const char *dev_name __rte_unused)
{
	return 0;
}

/**
 * The CUSE session is launched allowing the application to receive open,
 * release and ioctl calls.
 */
int
rte_vhost_driver_session_start(void)
{
	fuse_session_loop(session);

	return 0;
}
