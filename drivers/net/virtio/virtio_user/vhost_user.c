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

#include <rte_alarm.h>
#include <rte_string_fns.h>
#include <rte_fbarray.h>

#include "vhost.h"
#include "virtio_user_dev.h"

struct vhost_user_data {
	int vhostfd;
	int listenfd;
	uint64_t protocol_features;
};

#ifndef VHOST_USER_F_PROTOCOL_FEATURES
#define VHOST_USER_F_PROTOCOL_FEATURES 30
#endif

/** Protocol features. */
#ifndef VHOST_USER_PROTOCOL_F_MQ
#define VHOST_USER_PROTOCOL_F_MQ 0
#endif

#ifndef VHOST_USER_PROTOCOL_F_REPLY_ACK
#define VHOST_USER_PROTOCOL_F_REPLY_ACK 3
#endif

#ifndef VHOST_USER_PROTOCOL_F_STATUS
#define VHOST_USER_PROTOCOL_F_STATUS 16
#endif

#define VHOST_USER_SUPPORTED_PROTOCOL_FEATURES		\
	(1ULL << VHOST_USER_PROTOCOL_F_MQ |		\
	 1ULL << VHOST_USER_PROTOCOL_F_REPLY_ACK |	\
	 1ULL << VHOST_USER_PROTOCOL_F_STATUS)

/* The version of the protocol we support */
#define VHOST_USER_VERSION    0x1

#define VHOST_MEMORY_MAX_NREGIONS 8
struct vhost_memory {
	uint32_t nregions;
	uint32_t padding;
	struct vhost_memory_region regions[VHOST_MEMORY_MAX_NREGIONS];
};

enum vhost_user_request {
	VHOST_USER_NONE = 0,
	VHOST_USER_GET_FEATURES = 1,
	VHOST_USER_SET_FEATURES = 2,
	VHOST_USER_SET_OWNER = 3,
	VHOST_USER_RESET_OWNER = 4,
	VHOST_USER_SET_MEM_TABLE = 5,
	VHOST_USER_SET_LOG_BASE = 6,
	VHOST_USER_SET_LOG_FD = 7,
	VHOST_USER_SET_VRING_NUM = 8,
	VHOST_USER_SET_VRING_ADDR = 9,
	VHOST_USER_SET_VRING_BASE = 10,
	VHOST_USER_GET_VRING_BASE = 11,
	VHOST_USER_SET_VRING_KICK = 12,
	VHOST_USER_SET_VRING_CALL = 13,
	VHOST_USER_SET_VRING_ERR = 14,
	VHOST_USER_GET_PROTOCOL_FEATURES = 15,
	VHOST_USER_SET_PROTOCOL_FEATURES = 16,
	VHOST_USER_GET_QUEUE_NUM = 17,
	VHOST_USER_SET_VRING_ENABLE = 18,
	VHOST_USER_SET_STATUS = 39,
	VHOST_USER_GET_STATUS = 40,
	VHOST_USER_MAX
};

struct vhost_user_msg {
	enum vhost_user_request request;

#define VHOST_USER_VERSION_MASK     0x3
#define VHOST_USER_REPLY_MASK       (0x1 << 2)
#define VHOST_USER_NEED_REPLY_MASK  (0x1 << 3)
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
} __rte_packed;

#define VHOST_USER_HDR_SIZE offsetof(struct vhost_user_msg, payload.u64)
#define VHOST_USER_PAYLOAD_SIZE \
	(sizeof(struct vhost_user_msg) - VHOST_USER_HDR_SIZE)

static int
vhost_user_write(int fd, struct vhost_user_msg *msg, int *fds, int fd_num)
{
	int r;
	struct msghdr msgh;
	struct iovec iov;
	size_t fd_size = fd_num * sizeof(int);
	char control[CMSG_SPACE(fd_size)];
	struct cmsghdr *cmsg;

	memset(&msgh, 0, sizeof(msgh));
	memset(control, 0, sizeof(control));

	iov.iov_base = (uint8_t *)msg;
	iov.iov_len = VHOST_USER_HDR_SIZE + msg->size;

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

	if (r < 0)
		PMD_DRV_LOG(ERR, "Failed to send msg: %s", strerror(errno));

	return r;
}

static int
vhost_user_read(int fd, struct vhost_user_msg *msg)
{
	uint32_t valid_flags = VHOST_USER_REPLY_MASK | VHOST_USER_VERSION;
	int ret, sz_hdr = VHOST_USER_HDR_SIZE, sz_payload;

	ret = recv(fd, (void *)msg, sz_hdr, 0);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to recv msg header: %s", strerror(errno));
		return -1;
	} else if (ret < sz_hdr) {
		PMD_DRV_LOG(ERR, "Failed to recv msg hdr: %d instead of %d.",
			    ret, sz_hdr);
		return -1;
	}

	/* validate msg flags */
	if (msg->flags != (valid_flags)) {
		PMD_DRV_LOG(ERR, "Failed to recv msg: flags 0x%x instead of 0x%x.",
			    msg->flags, valid_flags);
		return -1;
	}

	sz_payload = msg->size;

	if ((size_t)sz_payload > sizeof(msg->payload)) {
		PMD_DRV_LOG(ERR, "Payload size overflow, header says %d but max %zu",
				sz_payload, sizeof(msg->payload));
		return -1;
	}

	if (sz_payload) {
		ret = recv(fd, (void *)((char *)msg + sz_hdr), sz_payload, 0);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to recv msg payload: %s", strerror(errno));
			return -1;
		} else if (ret < sz_payload) {
			PMD_DRV_LOG(ERR, "Failed to recv msg payload: %d instead of %u.",
				ret, msg->size);
			return -1;
		}
	}

	return 0;
}

static int
vhost_user_check_reply_ack(struct virtio_user_dev *dev, struct vhost_user_msg *msg)
{
	struct vhost_user_data *data = dev->backend_data;
	enum vhost_user_request req = msg->request;
	int ret;

	if (!(msg->flags & VHOST_USER_NEED_REPLY_MASK))
		return 0;

	ret = vhost_user_read(data->vhostfd, msg);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to read reply-ack");
		return -1;
	}

	if (req != msg->request) {
		PMD_DRV_LOG(ERR, "Unexpected reply-ack request type (%d)", msg->request);
		return -1;
	}

	if (msg->size != sizeof(msg->payload.u64)) {
		PMD_DRV_LOG(ERR, "Unexpected reply-ack payload size (%u)", msg->size);
		return -1;
	}

	if (msg->payload.u64) {
		PMD_DRV_LOG(ERR, "Slave replied NACK to request type (%d)", msg->request);
		return -1;
	}

	return 0;
}

static int
vhost_user_set_owner(struct virtio_user_dev *dev)
{
	int ret;
	struct vhost_user_data *data = dev->backend_data;
	struct vhost_user_msg msg = {
		.request = VHOST_USER_SET_OWNER,
		.flags = VHOST_USER_VERSION,
	};

	ret = vhost_user_write(data->vhostfd, &msg, NULL, 0);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to set owner");
		return -1;
	}

	return 0;
}

static int
vhost_user_get_protocol_features(struct virtio_user_dev *dev, uint64_t *features)
{
	int ret;
	struct vhost_user_data *data = dev->backend_data;
	struct vhost_user_msg msg = {
		.request = VHOST_USER_GET_PROTOCOL_FEATURES,
		.flags = VHOST_USER_VERSION,
	};

	ret = vhost_user_write(data->vhostfd, &msg, NULL, 0);
	if (ret < 0)
		goto err;

	ret = vhost_user_read(data->vhostfd, &msg);
	if (ret < 0)
		goto err;

	if (msg.request != VHOST_USER_GET_PROTOCOL_FEATURES) {
		PMD_DRV_LOG(ERR, "Unexpected request type (%d)", msg.request);
		goto err;
	}

	if (msg.size != sizeof(*features)) {
		PMD_DRV_LOG(ERR, "Unexpected payload size (%u)", msg.size);
		goto err;
	}

	*features = msg.payload.u64;

	return 0;
err:
	PMD_DRV_LOG(ERR, "Failed to get backend protocol features");

	return -1;
}

static int
vhost_user_set_protocol_features(struct virtio_user_dev *dev, uint64_t features)
{
	int ret;
	struct vhost_user_data *data = dev->backend_data;
	struct vhost_user_msg msg = {
		.request = VHOST_USER_SET_PROTOCOL_FEATURES,
		.flags = VHOST_USER_VERSION,
		.size = sizeof(features),
		.payload.u64 = features,
	};

	ret = vhost_user_write(data->vhostfd, &msg, NULL, 0);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to set protocol features");
		return -1;
	}

	return 0;
}

static int
vhost_user_get_features(struct virtio_user_dev *dev, uint64_t *features)
{
	int ret;
	struct vhost_user_data *data = dev->backend_data;
	struct vhost_user_msg msg = {
		.request = VHOST_USER_GET_FEATURES,
		.flags = VHOST_USER_VERSION,
	};

	ret = vhost_user_write(data->vhostfd, &msg, NULL, 0);
	if (ret < 0)
		goto err;

	ret = vhost_user_read(data->vhostfd, &msg);
	if (ret < 0)
		goto err;

	if (msg.request != VHOST_USER_GET_FEATURES) {
		PMD_DRV_LOG(ERR, "Unexpected request type (%d)", msg.request);
		goto err;
	}

	if (msg.size != sizeof(*features)) {
		PMD_DRV_LOG(ERR, "Unexpected payload size (%u)", msg.size);
		goto err;
	}

	*features = msg.payload.u64;

	if (!(*features & (1ULL << VHOST_USER_F_PROTOCOL_FEATURES)))
		return 0;

	/* Negotiate protocol features */
	ret = vhost_user_get_protocol_features(dev, &data->protocol_features);
	if (ret < 0)
		goto err;

	data->protocol_features &= VHOST_USER_SUPPORTED_PROTOCOL_FEATURES;

	ret = vhost_user_set_protocol_features(dev, data->protocol_features);
	if (ret < 0)
		goto err;

	if (!(data->protocol_features & (1ULL << VHOST_USER_PROTOCOL_F_MQ)))
		dev->unsupported_features |= (1ull << VIRTIO_NET_F_MQ);

	return 0;
err:
	PMD_DRV_LOG(ERR, "Failed to get backend features");

	return -1;
}

static int
vhost_user_set_features(struct virtio_user_dev *dev, uint64_t features)
{
	int ret;
	struct vhost_user_data *data = dev->backend_data;
	struct vhost_user_msg msg = {
		.request = VHOST_USER_SET_FEATURES,
		.flags = VHOST_USER_VERSION,
		.size = sizeof(features),
		.payload.u64 = features,
	};

	msg.payload.u64 |= dev->device_features & (1ULL << VHOST_USER_F_PROTOCOL_FEATURES);

	ret = vhost_user_write(data->vhostfd, &msg, NULL, 0);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to set features");
		return -1;
	}

	return 0;
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
vhost_user_set_memory_table(struct virtio_user_dev *dev)
{
	struct walk_arg wa;
	int fds[VHOST_MEMORY_MAX_NREGIONS];
	int ret, fd_num;
	struct vhost_user_data *data = dev->backend_data;
	struct vhost_user_msg msg = {
		.request = VHOST_USER_SET_MEM_TABLE,
		.flags = VHOST_USER_VERSION,
	};

	if (data->protocol_features & (1ULL << VHOST_USER_PROTOCOL_F_REPLY_ACK))
		msg.flags |= VHOST_USER_NEED_REPLY_MASK;

	wa.region_nr = 0;
	wa.vm = &msg.payload.memory;
	wa.fds = fds;

	/*
	 * The memory lock has already been taken by memory subsystem
	 * or virtio_user_start_device().
	 */
	ret = rte_memseg_walk_thread_unsafe(update_memory_region, &wa);
	if (ret < 0)
		goto err;

	fd_num = wa.region_nr;
	msg.payload.memory.nregions = wa.region_nr;
	msg.payload.memory.padding = 0;

	msg.size = sizeof(msg.payload.memory.nregions);
	msg.size += sizeof(msg.payload.memory.padding);
	msg.size += fd_num * sizeof(struct vhost_memory_region);

	ret = vhost_user_write(data->vhostfd, &msg, fds, fd_num);
	if (ret < 0)
		goto err;

	return vhost_user_check_reply_ack(dev, &msg);
err:
	PMD_DRV_LOG(ERR, "Failed to set memory table");
	return -1;
}

static int
vhost_user_set_vring(struct virtio_user_dev *dev, enum vhost_user_request req,
		struct vhost_vring_state *state)
{
	int ret;
	struct vhost_user_data *data = dev->backend_data;
	struct vhost_user_msg msg = {
		.request = req,
		.flags = VHOST_USER_VERSION,
		.size = sizeof(*state),
		.payload.state = *state,
	};

	ret = vhost_user_write(data->vhostfd, &msg, NULL, 0);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to set vring state (request %d)", req);
		return -1;
	}

	return 0;
}

static int
vhost_user_set_vring_enable(struct virtio_user_dev *dev, struct vhost_vring_state *state)
{
	return vhost_user_set_vring(dev, VHOST_USER_SET_VRING_ENABLE, state);
}

static int
vhost_user_set_vring_num(struct virtio_user_dev *dev, struct vhost_vring_state *state)
{
	return vhost_user_set_vring(dev, VHOST_USER_SET_VRING_NUM, state);
}

static int
vhost_user_set_vring_base(struct virtio_user_dev *dev, struct vhost_vring_state *state)
{
	return vhost_user_set_vring(dev, VHOST_USER_SET_VRING_BASE, state);
}

static int
vhost_user_get_vring_base(struct virtio_user_dev *dev, struct vhost_vring_state *state)
{
	int ret;
	struct vhost_user_msg msg;
	struct vhost_user_data *data = dev->backend_data;
	unsigned int index = state->index;

	ret = vhost_user_set_vring(dev, VHOST_USER_GET_VRING_BASE, state);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to send request");
		goto err;
	}

	ret = vhost_user_read(data->vhostfd, &msg);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to read reply");
		goto err;
	}

	if (msg.request != VHOST_USER_GET_VRING_BASE) {
		PMD_DRV_LOG(ERR, "Unexpected request type (%d)", msg.request);
		goto err;
	}

	if (msg.size != sizeof(*state)) {
		PMD_DRV_LOG(ERR, "Unexpected payload size (%u)", msg.size);
		goto err;
	}

	if (msg.payload.state.index != index) {
		PMD_DRV_LOG(ERR, "Unexpected ring index (%u)", state->index);
		goto err;
	}

	*state = msg.payload.state;

	return 0;
err:
	PMD_DRV_LOG(ERR, "Failed to get vring base");
	return -1;
}

static int
vhost_user_set_vring_file(struct virtio_user_dev *dev, enum vhost_user_request req,
		struct vhost_vring_file *file)
{
	int ret;
	int fd = file->fd;
	int num_fd = 0;
	struct vhost_user_data *data = dev->backend_data;
	struct vhost_user_msg msg = {
		.request = req,
		.flags = VHOST_USER_VERSION,
		.size = sizeof(msg.payload.u64),
		.payload.u64 = file->index & VHOST_USER_VRING_IDX_MASK,
	};

	if (fd >= 0)
		num_fd++;
	else
		msg.payload.u64 |= VHOST_USER_VRING_NOFD_MASK;

	ret = vhost_user_write(data->vhostfd, &msg, &fd, num_fd);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to set vring file (request %d)", req);
		return -1;
	}

	return 0;
}

static int
vhost_user_set_vring_call(struct virtio_user_dev *dev, struct vhost_vring_file *file)
{
	return vhost_user_set_vring_file(dev, VHOST_USER_SET_VRING_CALL, file);
}

static int
vhost_user_set_vring_kick(struct virtio_user_dev *dev, struct vhost_vring_file *file)
{
	return vhost_user_set_vring_file(dev, VHOST_USER_SET_VRING_KICK, file);
}


static int
vhost_user_set_vring_addr(struct virtio_user_dev *dev, struct vhost_vring_addr *addr)
{
	int ret;
	struct vhost_user_data *data = dev->backend_data;
	struct vhost_user_msg msg = {
		.request = VHOST_USER_SET_VRING_ADDR,
		.flags = VHOST_USER_VERSION,
		.size = sizeof(*addr),
		.payload.addr = *addr,
	};

	ret = vhost_user_write(data->vhostfd, &msg, NULL, 0);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to send vring addresses");
		return -1;
	}

	return 0;
}

static int
vhost_user_get_status(struct virtio_user_dev *dev, uint8_t *status)
{
	int ret;
	struct vhost_user_data *data = dev->backend_data;
	struct vhost_user_msg msg = {
		.request = VHOST_USER_GET_STATUS,
		.flags = VHOST_USER_VERSION,
	};

	/*
	 * If features have not been negotiated, we don't know if the backend
	 * supports protocol features
	 */
	if (!(dev->status & VIRTIO_CONFIG_STATUS_FEATURES_OK))
		return -ENOTSUP;

	/* Status protocol feature requires protocol features support */
	if (!(dev->device_features & (1ULL << VHOST_USER_F_PROTOCOL_FEATURES)))
		return -ENOTSUP;

	if (!(data->protocol_features & (1ULL << VHOST_USER_PROTOCOL_F_STATUS)))
		return -ENOTSUP;

	ret = vhost_user_write(data->vhostfd, &msg, NULL, 0);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to send request");
		goto err;
	}

	ret = vhost_user_read(data->vhostfd, &msg);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to recv request");
		goto err;
	}

	if (msg.request != VHOST_USER_GET_STATUS) {
		PMD_DRV_LOG(ERR, "Unexpected request type (%d)", msg.request);
		goto err;
	}

	if (msg.size != sizeof(msg.payload.u64)) {
		PMD_DRV_LOG(ERR, "Unexpected payload size (%u)", msg.size);
		goto err;
	}

	*status = (uint8_t)msg.payload.u64;

	return 0;
err:
	PMD_DRV_LOG(ERR, "Failed to get device status");
	return -1;
}

static int
vhost_user_set_status(struct virtio_user_dev *dev, uint8_t status)
{
	int ret;
	struct vhost_user_data *data = dev->backend_data;
	struct vhost_user_msg msg = {
		.request = VHOST_USER_SET_STATUS,
		.flags = VHOST_USER_VERSION,
		.size = sizeof(msg.payload.u64),
		.payload.u64 = status,
	};

	/*
	 * If features have not been negotiated, we don't know if the backend
	 * supports protocol features
	 */
	if (!(dev->status & VIRTIO_CONFIG_STATUS_FEATURES_OK))
		return -ENOTSUP;

	/* Status protocol feature requires protocol features support */
	if (!(dev->device_features & (1ULL << VHOST_USER_F_PROTOCOL_FEATURES)))
		return -ENOTSUP;

	if (!(data->protocol_features & (1ULL << VHOST_USER_PROTOCOL_F_STATUS)))
		return -ENOTSUP;

	if (data->protocol_features & (1ULL << VHOST_USER_PROTOCOL_F_REPLY_ACK))
		msg.flags |= VHOST_USER_NEED_REPLY_MASK;

	ret = vhost_user_write(data->vhostfd, &msg, NULL, 0);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to send get status request");
		return -1;
	}

	return vhost_user_check_reply_ack(dev, &msg);
}

#define MAX_VIRTIO_USER_BACKLOG 1
static int
vhost_user_start_server(struct virtio_user_dev *dev, struct sockaddr_un *un)
{
	int ret;
	int flag;
	struct vhost_user_data *data = dev->backend_data;
	int fd = data->listenfd;

	ret = bind(fd, (struct sockaddr *)un, sizeof(*un));
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "failed to bind to %s: %s; remove it and try again",
			    dev->path, strerror(errno));
		return -1;
	}
	ret = listen(fd, MAX_VIRTIO_USER_BACKLOG);
	if (ret < 0)
		return -1;

	PMD_DRV_LOG(NOTICE, "(%s) waiting for client connection...", dev->path);
	data->vhostfd = accept(fd, NULL, NULL);
	if (data->vhostfd < 0) {
		PMD_DRV_LOG(ERR, "Failed to accept initial client connection (%s)",
				strerror(errno));
		return -1;
	}

	flag = fcntl(fd, F_GETFL);
	if (fcntl(fd, F_SETFL, flag | O_NONBLOCK) < 0) {
		PMD_DRV_LOG(ERR, "fcntl failed, %s", strerror(errno));
		return -1;
	}

	return 0;
}

static int
vhost_user_server_disconnect(struct virtio_user_dev *dev)
{
	struct vhost_user_data *data = dev->backend_data;

	if (data->vhostfd < 0) {
		PMD_DRV_LOG(ERR, "(%s) Expected valid Vhost FD", dev->path);
		return -1;
	}

	close(data->vhostfd);
	data->vhostfd = -1;

	return 0;
}

static int
vhost_user_server_reconnect(struct virtio_user_dev *dev)
{
	struct vhost_user_data *data = dev->backend_data;
	int fd;

	fd = accept(data->listenfd, NULL, NULL);
	if (fd < 0)
		return -1;

	data->vhostfd = fd;

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
	struct vhost_user_data *data;

	data = malloc(sizeof(*data));
	if (!data) {
		PMD_DRV_LOG(ERR, "(%s) Failed to allocate Vhost-user data", dev->path);
		return -1;
	}

	memset(data, 0, sizeof(*data));

	dev->backend_data = data;

	data->vhostfd = -1;
	data->listenfd = -1;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		PMD_DRV_LOG(ERR, "socket() error, %s", strerror(errno));
		goto err_data;
	}

	flag = fcntl(fd, F_GETFD);
	if (flag == -1)
		PMD_DRV_LOG(WARNING, "fcntl get fd failed, %s", strerror(errno));
	else if (fcntl(fd, F_SETFD, flag | FD_CLOEXEC) < 0)
		PMD_DRV_LOG(WARNING, "fcntl set fd failed, %s", strerror(errno));

	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	strlcpy(un.sun_path, dev->path, sizeof(un.sun_path));

	if (dev->is_server) {
		data->listenfd = fd;
		if (vhost_user_start_server(dev, &un) < 0) {
			PMD_DRV_LOG(ERR, "virtio-user startup fails in server mode");
			goto err_socket;
		}
	} else {
		if (connect(fd, (struct sockaddr *)&un, sizeof(un)) < 0) {
			PMD_DRV_LOG(ERR, "connect error, %s", strerror(errno));
			goto err_socket;
		}
		data->vhostfd = fd;
	}

	return 0;

err_socket:
	close(fd);
err_data:
	free(data);
	dev->backend_data = NULL;

	return -1;
}

static int
vhost_user_destroy(struct virtio_user_dev *dev)
{
	struct vhost_user_data *data = dev->backend_data;

	if (!data)
		return 0;

	if (data->vhostfd >= 0) {
		close(data->vhostfd);
		data->vhostfd = -1;
	}

	if (data->listenfd >= 0) {
		close(data->listenfd);
		data->listenfd = -1;
	}

	free(data);
	dev->backend_data = NULL;

	return 0;
}

static int
vhost_user_enable_queue_pair(struct virtio_user_dev *dev,
			     uint16_t pair_idx,
			     int enable)
{
	struct vhost_user_data *data = dev->backend_data;
	int i;

	if (data->vhostfd < 0)
		return 0;

	if (dev->qp_enabled[pair_idx] == enable)
		return 0;

	for (i = 0; i < 2; ++i) {
		struct vhost_vring_state state = {
			.index = pair_idx * 2 + i,
			.num = enable,
		};

		if (vhost_user_set_vring_enable(dev, &state))
			return -1;
	}

	dev->qp_enabled[pair_idx] = enable;
	return 0;
}

static int
vhost_user_get_backend_features(uint64_t *features)
{
	*features = 1ULL << VHOST_USER_F_PROTOCOL_FEATURES;

	return 0;
}

static int
vhost_user_update_link_state(struct virtio_user_dev *dev)
{
	struct vhost_user_data *data = dev->backend_data;
	char buf[128];

	if (data->vhostfd >= 0) {
		int r;
		int flags;

		flags = fcntl(data->vhostfd, F_GETFL);
		if (fcntl(data->vhostfd, F_SETFL, flags | O_NONBLOCK) == -1) {
			PMD_DRV_LOG(ERR, "error setting O_NONBLOCK flag");
			return -1;
		}

		r = recv(data->vhostfd, buf, 128, MSG_PEEK);
		if (r == 0 || (r < 0 && errno != EAGAIN)) {
			dev->net_status &= (~VIRTIO_NET_S_LINK_UP);
			PMD_DRV_LOG(ERR, "virtio-user port %u is down", dev->hw.port_id);

			/* This function could be called in the process
			 * of interrupt handling, callback cannot be
			 * unregistered here, set an alarm to do it.
			 */
			rte_eal_alarm_set(1,
				virtio_user_dev_delayed_disconnect_handler,
				(void *)dev);
		} else {
			dev->net_status |= VIRTIO_NET_S_LINK_UP;
		}

		if (fcntl(data->vhostfd, F_SETFL,
					flags & ~O_NONBLOCK) == -1) {
			PMD_DRV_LOG(ERR, "error clearing O_NONBLOCK flag");
			return -1;
		}
	} else if (dev->is_server) {
		dev->net_status &= (~VIRTIO_NET_S_LINK_UP);
		if (virtio_user_dev_server_reconnect(dev) >= 0)
			dev->net_status |= VIRTIO_NET_S_LINK_UP;
	}

	return 0;
}

static int
vhost_user_get_intr_fd(struct virtio_user_dev *dev)
{
	struct vhost_user_data *data = dev->backend_data;

	if (dev->is_server && data->vhostfd == -1)
		return data->listenfd;

	return data->vhostfd;
}

struct virtio_user_backend_ops virtio_ops_user = {
	.setup = vhost_user_setup,
	.destroy = vhost_user_destroy,
	.get_backend_features = vhost_user_get_backend_features,
	.set_owner = vhost_user_set_owner,
	.get_features = vhost_user_get_features,
	.set_features = vhost_user_set_features,
	.set_memory_table = vhost_user_set_memory_table,
	.set_vring_num = vhost_user_set_vring_num,
	.set_vring_base = vhost_user_set_vring_base,
	.get_vring_base = vhost_user_get_vring_base,
	.set_vring_call = vhost_user_set_vring_call,
	.set_vring_kick = vhost_user_set_vring_kick,
	.set_vring_addr = vhost_user_set_vring_addr,
	.get_status = vhost_user_get_status,
	.set_status = vhost_user_set_status,
	.enable_qp = vhost_user_enable_queue_pair,
	.update_link_state = vhost_user_update_link_state,
	.server_disconnect = vhost_user_server_disconnect,
	.server_reconnect = vhost_user_server_reconnect,
	.get_intr_fd = vhost_user_get_intr_fd,
};
