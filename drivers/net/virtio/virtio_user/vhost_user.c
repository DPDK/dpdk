/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>

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

struct hugepage_file_info {
	uint64_t addr;            /**< virtual addr */
	size_t   size;            /**< the file size */
	char     path[PATH_MAX];  /**< path to backing file */
};

/* Two possible options:
 * 1. Match HUGEPAGE_INFO_FMT to find the file storing struct hugepage_file
 * array. This is simple but cannot be used in secondary process because
 * secondary process will close and munmap that file.
 * 2. Match HUGEFILE_FMT to find hugepage files directly.
 *
 * We choose option 2.
 */
static int
get_hugepage_file_info(struct hugepage_file_info huges[], int max)
{
	int idx;
	FILE *f;
	char buf[BUFSIZ], *tmp, *tail;
	char *str_underline, *str_start;
	int huge_index;
	uint64_t v_start, v_end;

	f = fopen("/proc/self/maps", "r");
	if (!f) {
		PMD_DRV_LOG(ERR, "cannot open /proc/self/maps");
		return -1;
	}

	idx = 0;
	while (fgets(buf, sizeof(buf), f) != NULL) {
		if (sscanf(buf, "%" PRIx64 "-%" PRIx64, &v_start, &v_end) < 2) {
			PMD_DRV_LOG(ERR, "Failed to parse address");
			goto error;
		}

		tmp = strchr(buf, ' ') + 1; /** skip address */
		tmp = strchr(tmp, ' ') + 1; /** skip perm */
		tmp = strchr(tmp, ' ') + 1; /** skip offset */
		tmp = strchr(tmp, ' ') + 1; /** skip dev */
		tmp = strchr(tmp, ' ') + 1; /** skip inode */
		while (*tmp == ' ')         /** skip spaces */
			tmp++;
		tail = strrchr(tmp, '\n');  /** remove newline if exists */
		if (tail)
			*tail = '\0';

		/* Match HUGEFILE_FMT, aka "%s/%smap_%d",
		 * which is defined in eal_filesystem.h
		 */
		str_underline = strrchr(tmp, '_');
		if (!str_underline)
			continue;

		str_start = str_underline - strlen("map");
		if (str_start < tmp)
			continue;

		if (sscanf(str_start, "map_%d", &huge_index) != 1)
			continue;

		if (idx >= max) {
			PMD_DRV_LOG(ERR, "Exceed maximum of %d", max);
			goto error;
		}
		huges[idx].addr = v_start;
		huges[idx].size = v_end - v_start;
		snprintf(huges[idx].path, PATH_MAX, "%s", tmp);
		idx++;
	}

	fclose(f);
	return idx;

error:
	fclose(f);
	return -1;
}

static int
prepare_vhost_memory_user(struct vhost_user_msg *msg, int fds[])
{
	int i, num;
	struct hugepage_file_info huges[VHOST_MEMORY_MAX_NREGIONS];
	struct vhost_memory_region *mr;

	num = get_hugepage_file_info(huges, VHOST_MEMORY_MAX_NREGIONS);
	if (num < 0) {
		PMD_INIT_LOG(ERR, "Failed to prepare memory for vhost-user");
		return -1;
	}

	for (i = 0; i < num; ++i) {
		mr = &msg->payload.memory.regions[i];
		mr->guest_phys_addr = huges[i].addr; /* use vaddr! */
		mr->userspace_addr = huges[i].addr;
		mr->memory_size = huges[i].size;
		mr->mmap_offset = 0;
		fds[i] = open(huges[i].path, O_RDWR);
	}

	msg->payload.memory.nregions = num;
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
	int i, len;
	int vhostfd = dev->vhostfd;

	RTE_SET_USED(m);

	PMD_DRV_LOG(INFO, "%s", vhost_msg_strings[req]);

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

	if (req == VHOST_USER_SET_MEM_TABLE)
		for (i = 0; i < fd_num; ++i)
			close(fds[i]);

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
	snprintf(un.sun_path, sizeof(un.sun_path), "%s", dev->path);
	if (connect(fd, (struct sockaddr *)&un, sizeof(un)) < 0) {
		PMD_DRV_LOG(ERR, "connect error, %s", strerror(errno));
		close(fd);
		return -1;
	}

	dev->vhostfd = fd;
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

struct virtio_user_backend_ops ops_user = {
	.setup = vhost_user_setup,
	.send_request = vhost_user_sock,
	.enable_qp = vhost_user_enable_queue_pair
};
