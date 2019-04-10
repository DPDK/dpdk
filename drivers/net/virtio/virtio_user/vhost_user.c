/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>

#include <rte_string_fns.h>
#include <rte_fbarray.h>
#include <rte_eal_memconfig.h>

#include "vhost.h"
#include "virtio_user_dev.h"

/* The version of the protocol we support */
#define VHOST_USER_VERSION    0x1

#define VHOST_MEMORY_MAX_NREGIONS 8
struct vhost_memory {
	uint32_t nregions;
	uint32_t padding;
	struct vhost_memory_region regions[VHOST_MEMORY_MAX_NREGIONS];
};

struct vhost_user_msg {
	enum vhost_user_request request;

#define VHOST_USER_VERSION_MASK     0x3
#define VHOST_USER_REPLY_MASK       (0x1 << 2)
	uint32_t flags;
	uint32_t size; /* the following payload size */
	union {
#define VHOST_USER_VRING_IDX_MASK   0xff
#define VHOST_USER_VRING_NOFD_MASK  (0x1 << 8)
		uint64_t u64;
		struct vhost_vring_state state;
		struct vhost_vring_addr addr;
		struct vhost_memory memory;
	} payload;
	int fds[VHOST_MEMORY_MAX_NREGIONS];
} __attribute((packed));

#define VHOST_USER_HDR_SIZE offsetof(struct vhost_user_msg, payload.u64)
#define VHOST_USER_PAYLOAD_SIZE \
	(sizeof(struct vhost_user_msg) - VHOST_USER_HDR_SIZE)

static int
vhost_user_write(int fd, void *buf, int len, int *fds, int fd_num)
{
	int r;
	struct msghdr msgh;
	struct iovec iov;
	size_t fd_size = fd_num * sizeof(int);
	char control[CMSG_SPACE(fd_size)];
	struct cmsghdr *cmsg;

	memset(&msgh, 0, sizeof(msgh));
	memset(control, 0, sizeof(control));

	iov.iov_base = (uint8_t *)buf;
	iov.iov_len = len;

	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;
	msgh.msg_control = control;
	msgh.msg_controllen = sizeof(control);

	cmsg = CMSG_FIRSTHDR(&msgh);
	cmsg->cmsg_len = CMSG_LEN(fd_size);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	memcpy(CMSG_DATA(cmsg), fds, fd_size);

	do {
		r = sendmsg(fd, &msgh, 0);
	} while (r < 0 && errno == EINTR);

	return r;
}

static int
vhost_user_read(int fd, struct vhost_user_msg *msg)
{
	uint32_t valid_flags = VHOST_USER_REPLY_MASK | VHOST_USER_VERSION;
	int ret, sz_hdr = VHOST_USER_HDR_SIZE, sz_payload;

	ret = recv(fd, (void *)msg, sz_hdr, 0);
	if (ret < sz_hdr) {
		PMD_DRV_LOG(ERR, "Failed to recv msg hdr: %d instead of %d.",
			    ret, sz_hdr);
		goto fail;
	}

	/* validate msg flags */
	if (msg->flags != (valid_flags)) {
		PMD_DRV_LOG(ERR, "Failed to recv msg: flags %x instead of %x.",
			    msg->flags, valid_flags);
		goto fail;
	}

	sz_payload = msg->size;

	if ((size_t)sz_payload > sizeof(msg->payload))
		goto fail;

	if (sz_payload) {
		ret = recv(fd, (void *)((char *)msg + sz_hdr), sz_payload, 0);
		if (ret < sz_payload) {
			PMD_DRV_LOG(ERR,
				"Failed to recv msg payload: %d instead of %d.",
				ret, msg->size);
			goto fail;
		}
	}

	return 0;

fail:
	return -1;
}

struct walk_arg {
	struct vhost_memory *vm;
	int *fds;
	int region_nr;
};

static int
update_memory_region(const struct rte_memseg_list *msl __rte_unused,
		const struct rte_memseg *ms, void *arg)
{
	struct walk_arg *wa = arg;
	struct vhost_memory_region *mr;
	uint64_t start_addr, end_addr;
	size_t offset;
	int i, fd;

	fd = rte_memseg_get_fd_thread_unsafe(ms);
	if (fd < 0) {
		PMD_DRV_LOG(ERR, "Failed to get fd, ms=%p rte_errno=%d",
			ms, rte_errno);
		return -1;
	}

	if (rte_memseg_get_fd_offset_thread_unsafe(ms, &offset) < 0) {
		PMD_DRV_LOG(ERR, "Failed to get offset, ms=%p rte_errno=%d",
			ms, rte_errno);
		return -1;
	}

	start_addr = (uint64_t)(uintptr_t)ms->addr;
	end_addr = start_addr + ms->len;

	for (i = 0; i < wa->region_nr; i++) {
		if (wa->fds[i] != fd)
			continue;

		mr = &wa->vm->regions[i];

		if (mr->userspace_addr + mr->memory_size < end_addr)
			mr->memory_size = end_addr - mr->userspace_addr;

		if (mr->userspace_addr > start_addr) {
			mr->userspace_addr = start_addr;
			mr->guest_phys_addr = start_addr;
		}

		if (mr->mmap_offset > offset)
			mr->mmap_offset = offset;

		PMD_DRV_LOG(DEBUG, "index=%d fd=%d offset=0x%" PRIx64
			" addr=0x%" PRIx64 " len=%" PRIu64, i, fd,
			mr->mmap_offset, mr->userspace_addr,
			mr->memory_size);

		return 0;
	}

	if (i >= VHOST_MEMORY_MAX_NREGIONS) {
		PMD_DRV_LOG(ERR, "Too many memory regions");
		return -1;
	}

	mr = &wa->vm->regions[i];
	wa->fds[i] = fd;

	mr->guest_phys_addr = start_addr;
	mr->userspace_addr = start_addr;
	mr->memory_size = ms->len;
	mr->mmap_offset = offset;

	PMD_DRV_LOG(DEBUG, "index=%d fd=%d offset=0x%" PRIx64
		" addr=0x%" PRIx64 " len=%" PRIu64, i, fd,
		mr->mmap_offset, mr->userspace_addr,
		mr->memory_size);

	wa->region_nr++;

	return 0;
}

static int
prepare_vhost_memory_user(struct vhost_user_msg *msg, int fds[])
{
	struct walk_arg wa;

	wa.region_nr = 0;
	wa.vm = &msg->payload.memory;
	wa.fds = fds;

	/*
	 * The memory lock has already been taken by memory subsystem
	 * or virtio_user_start_device().
	 */
	if (rte_memseg_walk_thread_unsafe(update_memory_region, &wa) < 0)
		return -1;

	msg->payload.memory.nregions = wa.region_nr;
	msg->payload.memory.padding = 0;

	return 0;
}

static struct vhost_user_msg m;

const char * const vhost_msg_strings[] = {
	[VHOST_USER_SET_OWNER] = "VHOST_SET_OWNER",
	[VHOST_USER_RESET_OWNER] = "VHOST_RESET_OWNER",
	[VHOST_USER_SET_FEATURES] = "VHOST_SET_FEATURES",
	[VHOST_USER_GET_FEATURES] = "VHOST_GET_FEATURES",
	[VHOST_USER_SET_VRING_CALL] = "VHOST_SET_VRING_CALL",
	[VHOST_USER_SET_VRING_NUM] = "VHOST_SET_VRING_NUM",
	[VHOST_USER_SET_VRING_BASE] = "VHOST_SET_VRING_BASE",
	[VHOST_USER_GET_VRING_BASE] = "VHOST_GET_VRING_BASE",
	[VHOST_USER_SET_VRING_ADDR] = "VHOST_SET_VRING_ADDR",
	[VHOST_USER_SET_VRING_KICK] = "VHOST_SET_VRING_KICK",
	[VHOST_USER_SET_MEM_TABLE] = "VHOST_SET_MEM_TABLE",
	[VHOST_USER_SET_VRING_ENABLE] = "VHOST_SET_VRING_ENABLE",
};

static int
vhost_user_sock(struct virtio_user_dev *dev,
		enum vhost_user_request req,
		void *arg)
{
	struct vhost_user_msg msg;
	struct vhost_vring_file *file = 0;
	int need_reply = 0;
	int fds[VHOST_MEMORY_MAX_NREGIONS];
	int fd_num = 0;
	int len;
	int vhostfd = dev->vhostfd;

	RTE_SET_USED(m);

	PMD_DRV_LOG(INFO, "%s", vhost_msg_strings[req]);

	if (dev->is_server && vhostfd < 0)
		return -1;

	msg.request = req;
	msg.flags = VHOST_USER_VERSION;
	msg.size = 0;

	switch (req) {
	case VHOST_USER_GET_FEATURES:
		need_reply = 1;
		break;

	case VHOST_USER_SET_FEATURES:
	case VHOST_USER_SET_LOG_BASE:
		msg.payload.u64 = *((__u64 *)arg);
		msg.size = sizeof(m.payload.u64);
		break;

	case VHOST_USER_SET_OWNER:
	case VHOST_USER_RESET_OWNER:
		break;

	case VHOST_USER_SET_MEM_TABLE:
		if (prepare_vhost_memory_user(&msg, fds) < 0)
			return -1;
		fd_num = msg.payload.memory.nregions;
		msg.size = sizeof(m.payload.memory.nregions);
		msg.size += sizeof(m.payload.memory.padding);
		msg.size += fd_num * sizeof(struct vhost_memory_region);
		break;

	case VHOST_USER_SET_LOG_FD:
		fds[fd_num++] = *((int *)arg);
		break;

	case VHOST_USER_SET_VRING_NUM:
	case VHOST_USER_SET_VRING_BASE:
	case VHOST_USER_SET_VRING_ENABLE:
		memcpy(&msg.payload.state, arg, sizeof(msg.payload.state));
		msg.size = sizeof(m.payload.state);
		break;

	case VHOST_USER_GET_VRING_BASE:
		memcpy(&msg.payload.state, arg, sizeof(msg.payload.state));
		msg.size = sizeof(m.payload.state);
		need_reply = 1;
		break;

	case VHOST_USER_SET_VRING_ADDR:
		memcpy(&msg.payload.addr, arg, sizeof(msg.payload.addr));
		msg.size = sizeof(m.payload.addr);
		break;

	case VHOST_USER_SET_VRING_KICK:
	case VHOST_USER_SET_VRING_CALL:
	case VHOST_USER_SET_VRING_ERR:
		file = arg;
		msg.payload.u64 = file->index & VHOST_USER_VRING_IDX_MASK;
		msg.size = sizeof(m.payload.u64);
		if (file->fd > 0)
			fds[fd_num++] = file->fd;
		else
			msg.payload.u64 |= VHOST_USER_VRING_NOFD_MASK;
		break;

	default:
		PMD_DRV_LOG(ERR, "trying to send unhandled msg type");
		return -1;
	}

	len = VHOST_USER_HDR_SIZE + msg.size;
	if (vhost_user_write(vhostfd, &msg, len, fds, fd_num) < 0) {
		PMD_DRV_LOG(ERR, "%s failed: %s",
			    vhost_msg_strings[req], strerror(errno));
		return -1;
	}

	if (need_reply) {
		if (vhost_user_read(vhostfd, &msg) < 0) {
			PMD_DRV_LOG(ERR, "Received msg failed: %s",
				    strerror(errno));
			return -1;
		}

		if (req != msg.request) {
			PMD_DRV_LOG(ERR, "Received unexpected msg type");
			return -1;
		}

		switch (req) {
		case VHOST_USER_GET_FEATURES:
			if (msg.size != sizeof(m.payload.u64)) {
				PMD_DRV_LOG(ERR, "Received bad msg size");
				return -1;
			}
			*((__u64 *)arg) = msg.payload.u64;
			break;
		case VHOST_USER_GET_VRING_BASE:
			if (msg.size != sizeof(m.payload.state)) {
				PMD_DRV_LOG(ERR, "Received bad msg size");
				return -1;
			}
			memcpy(arg, &msg.payload.state,
			       sizeof(struct vhost_vring_state));
			break;
		default:
			PMD_DRV_LOG(ERR, "Received unexpected msg type");
			return -1;
		}
	}

	return 0;
}

#define MAX_VIRTIO_USER_BACKLOG 1
static int
virtio_user_start_server(struct virtio_user_dev *dev, struct sockaddr_un *un)
{
	int ret;
	int flag;
	int fd = dev->listenfd;

	ret = bind(fd, (struct sockaddr *)un, sizeof(*un));
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "failed to bind to %s: %s; remove it and try again\n",
			    dev->path, strerror(errno));
		return -1;
	}
	ret = listen(fd, MAX_VIRTIO_USER_BACKLOG);
	if (ret < 0)
		return -1;

	flag = fcntl(fd, F_GETFL);
	if (fcntl(fd, F_SETFL, flag | O_NONBLOCK) < 0) {
		PMD_DRV_LOG(ERR, "fcntl failed, %s", strerror(errno));
		return -1;
	}

	return 0;
}

/**
 * Set up environment to talk with a vhost user backend.
 *
 * @return
 *   - (-1) if fail;
 *   - (0) if succeed.
 */
static int
vhost_user_setup(struct virtio_user_dev *dev)
{
	int fd;
	int flag;
	struct sockaddr_un un;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		PMD_DRV_LOG(ERR, "socket() error, %s", strerror(errno));
		return -1;
	}

	flag = fcntl(fd, F_GETFD);
	if (fcntl(fd, F_SETFD, flag | FD_CLOEXEC) < 0)
		PMD_DRV_LOG(WARNING, "fcntl failed, %s", strerror(errno));

	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	strlcpy(un.sun_path, dev->path, sizeof(un.sun_path));

	if (dev->is_server) {
		dev->listenfd = fd;
		if (virtio_user_start_server(dev, &un) < 0) {
			PMD_DRV_LOG(ERR, "virtio-user startup fails in server mode");
			close(fd);
			return -1;
		}
		dev->vhostfd = -1;
	} else {
		if (connect(fd, (struct sockaddr *)&un, sizeof(un)) < 0) {
			PMD_DRV_LOG(ERR, "connect error, %s", strerror(errno));
			close(fd);
			return -1;
		}
		dev->vhostfd = fd;
	}

	return 0;
}

static int
vhost_user_enable_queue_pair(struct virtio_user_dev *dev,
			     uint16_t pair_idx,
			     int enable)
{
	int i;

	for (i = 0; i < 2; ++i) {
		struct vhost_vring_state state = {
			.index = pair_idx * 2 + i,
			.num   = enable,
		};

		if (vhost_user_sock(dev, VHOST_USER_SET_VRING_ENABLE, &state))
			return -1;
	}

	return 0;
}

struct virtio_user_backend_ops virtio_ops_user = {
	.setup = vhost_user_setup,
	.send_request = vhost_user_sock,
	.enable_qp = vhost_user_enable_queue_pair
};
