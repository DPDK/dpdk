/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>

#include <rte_log.h>

#include "virtio_ha.h"

RTE_LOG_REGISTER(virtio_ha_ipc_logtype, pmd.vdpa.virtio, NOTICE);
#define HA_IPC_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, virtio_ha_ipc_logtype, \
		"VIRTIO HA IPC %s(): " fmt "\n", __func__, ##args)

static int ipc_client_sock;
static bool ipc_client_connected;

struct virtio_ha_msg *
virtio_ha_alloc_msg(void)
{
	struct virtio_ha_msg *msg;
	int i;

	msg = malloc(sizeof(struct virtio_ha_msg));
	if (msg == NULL) {
		HA_IPC_LOG(ERR, "Failed to alloc HA message");
		return NULL;
	}

	memset(msg, 0, sizeof(struct virtio_ha_msg));

	for (i = 0; i < VIRTIO_HA_MAX_FDS; i++)
		msg->fds[i] = -1;

	return msg;
}

inline void
virtio_ha_free_msg(struct virtio_ha_msg *msg)
{
	/* Assume fds will be used later, so don't close fd here */
	free(msg);
}

void
virtio_ha_reset_msg(struct virtio_ha_msg *msg)
{
	int i;

	memset(msg, 0, sizeof(struct virtio_ha_msg));

	for (i = 0; i < VIRTIO_HA_MAX_FDS; i++)
		msg->fds[i] = -1;	
}

static void
close_msg_fds(struct virtio_ha_msg *msg)
{
	int i;

	for (i = 0; i < msg->nr_fds; i++) {
		int fd = msg->fds[i];

		if (fd == -1)
			continue;

		msg->fds[i] = -1;
		close(fd);
	}
}

/*
 * return bytes# of read on success or negative val on failure. Update fdnum
 * with number of fds read.
 */
static int
read_fd_message(int sockfd, char *buf, int buflen, int *fds, int max_fds, int *fd_num)
{
	struct iovec iov;
	struct msghdr msgh;
	char control[CMSG_SPACE(max_fds * sizeof(int))];
	struct cmsghdr *cmsg;
	int got_fds = 0;
	int ret;

	*fd_num = 0;

	memset(&msgh, 0, sizeof(msgh));
	iov.iov_base = buf;
	iov.iov_len  = buflen;

	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;
	msgh.msg_control = control;
	msgh.msg_controllen = sizeof(control);

	ret = recvmsg(sockfd, &msgh, 0);
	if (ret <= 0) {
		if (ret)
			HA_IPC_LOG(ERR, "Failed to recvmsg on fd %d (%s)",
				sockfd, strerror(errno));
		return ret;
	}

	if (msgh.msg_flags & MSG_TRUNC)
		HA_IPC_LOG(ERR, "Truncated message (fd %d)", sockfd);

	/* MSG_CTRUNC may be caused by LSM misconfiguration */
	if (msgh.msg_flags & MSG_CTRUNC)
		HA_IPC_LOG(ERR, "Truncated control data (fd %d)", sockfd);

	for (cmsg = CMSG_FIRSTHDR(&msgh); cmsg != NULL;
		cmsg = CMSG_NXTHDR(&msgh, cmsg)) {
		if ((cmsg->cmsg_level == SOL_SOCKET) &&
			(cmsg->cmsg_type == SCM_RIGHTS)) {
			got_fds = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
			*fd_num = got_fds;
			memcpy(fds, CMSG_DATA(cmsg), got_fds * sizeof(int));
			break;
		}
	}

	/* Clear out unused file descriptors */
	while (got_fds < max_fds)
		fds[got_fds++] = -1;

	return ret;
}

static int
send_fd_message(int sockfd, struct iovec *iov, size_t nr_iov, int *fds, int fd_num, size_t msg_sz)
{
	struct msghdr msgh;
	size_t fdsize = fd_num * sizeof(int);
	char control[CMSG_SPACE(fdsize)];
	struct cmsghdr *cmsg;
	int ret;

	memset(&msgh, 0, sizeof(msgh));

	msgh.msg_iov = iov;
	msgh.msg_iovlen = nr_iov;

	if (fds && fd_num > 0) {
		msgh.msg_control = control;
		msgh.msg_controllen = sizeof(control);
		cmsg = CMSG_FIRSTHDR(&msgh);
		if (cmsg == NULL) {
			HA_IPC_LOG(ERR, "Missing cmsg");
			errno = EINVAL;
			return -1;
		}
		cmsg->cmsg_len = CMSG_LEN(fdsize);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		memcpy(CMSG_DATA(cmsg), fds, fdsize);
	} else {
		msgh.msg_control = NULL;
		msgh.msg_controllen = 0;
	}

	do {
		ret = sendmsg(sockfd, &msgh, MSG_NOSIGNAL);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to sendmsg on fd %d (%s)",
			sockfd, strerror(errno));
		return ret;
	} else if (ret != (int)msg_sz) {
		HA_IPC_LOG(ERR, "Failed to send complete msg on fd %d", sockfd);
		return -1;		
	}

	return ret;
}

int
virtio_ha_send_msg(int sockfd, struct virtio_ha_msg *msg)
{
	struct virtio_ha_msg_hdr *hdr = &msg->hdr;
	struct iovec iov[2];
	size_t nr_iov = 1, msg_sz = 0;

	iov[0].iov_base = hdr;
	iov[0].iov_len = sizeof(*hdr);
	msg_sz += sizeof(*hdr);

	if (hdr->size) {
		iov[1].iov_base = msg->iov.iov_base;
		iov[1].iov_len = msg->iov.iov_len;
		msg_sz += msg->iov.iov_len;
		nr_iov++;
	}

	return send_fd_message(sockfd, iov, nr_iov, msg->fds, msg->nr_fds, msg_sz);
}

int
virtio_ha_recv_msg(int sockfd, struct virtio_ha_msg *msg)
{
	struct virtio_ha_msg_hdr *hdr = &msg->hdr;
	int ret;

	ret = read_fd_message(sockfd, (char *)hdr, sizeof(*hdr),
		msg->fds, VIRTIO_HA_MAX_FDS, &msg->nr_fds);
	if (ret <= 0)
		goto out;

	if (ret != sizeof(*hdr)) {
		HA_IPC_LOG(ERR, "Unexpected header size read\n");
		ret = -1;
		goto out;
	}

	if (hdr->size) {
		msg->iov.iov_len = hdr->size;
		msg->iov.iov_base = malloc(msg->iov.iov_len);
		if (msg->iov.iov_base == NULL) {
			HA_IPC_LOG(ERR, "Failed to alloc message payload when read message");
			ret = -1;
			goto out;
		}
		ret = read(sockfd, msg->iov.iov_base, msg->iov.iov_len);
		if (ret <= 0)
			goto out;
		if (ret != (int)msg->iov.iov_len) {
			HA_IPC_LOG(ERR, "Failed to read complete message payload (fd %d)", sockfd);
			ret = -1;
			goto out;
		}
	}

out:
	if (ret <= 0)
		close_msg_fds(msg);

	return ret;
}

static void *
ipc_connection_handler(__rte_unused void *ptr)
{
	struct epoll_event event, ev;
	struct sockaddr_un addr;
	int epfd, nev;

	epfd = epoll_create(1);
	if (epfd < 0) {
		HA_IPC_LOG(ERR, "Failed to create epoll fd");
		return NULL;
	}

	event.events = EPOLLHUP | EPOLLERR;

	if (__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED) &&
		epoll_ctl(epfd, EPOLL_CTL_ADD, ipc_client_sock, &event) < 0) {
		HA_IPC_LOG(ERR, "Failed to epoll ctl add");
		return NULL;
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, VIRTIO_HA_UDS_PATH);

	while (1) {
		if (__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED)) {
			nev = epoll_wait(epfd, &ev, 1, -1);
			if (nev == 1) {
				if ((ev.events & EPOLLERR) || (ev.events & EPOLLHUP)) {
					HA_IPC_LOG(ERR, "Disconnected from ipc server");
					__atomic_store_n(&ipc_client_connected, false, __ATOMIC_RELAXED);

					if (epoll_ctl(epfd, EPOLL_CTL_DEL, ipc_client_sock, &event) < 0)
						HA_IPC_LOG(ERR, "Failed to epoll ctl del for fd %d", ipc_client_sock);
					close(ipc_client_sock);

					ipc_client_sock = socket(AF_UNIX, SOCK_STREAM, 0);
					if (ipc_client_sock < 0) {
						HA_IPC_LOG(ERR, "Failed to re-create ipc client socket");
						return NULL;
					}
				}			
			}
		} else {
			if (connect(ipc_client_sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
				HA_IPC_LOG(INFO, "Reconnected to ipc server, starting HA mode");
				if (epoll_ctl(epfd, EPOLL_CTL_ADD, ipc_client_sock, &event) < 0) {
					HA_IPC_LOG(ERR, "Failed to epoll ctl add for re-created sock");
					return NULL;
				}
				__atomic_store_n(&ipc_client_connected, true, __ATOMIC_RELAXED);
			} else {
				sleep(1);
			}
		}
    }
}

int
virtio_ha_ipc_client_init(void)
{
	struct sockaddr_un addr;
	pthread_t thread;

	ipc_client_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ipc_client_sock < 0) {
		HA_IPC_LOG(ERR, "Failed to create ipc client socket");
		return -1;
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, VIRTIO_HA_UDS_PATH);
	if (connect(ipc_client_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		HA_IPC_LOG(INFO, "Failed to connect to ipc server, starting non-HA mode");
		__atomic_store_n(&ipc_client_connected, false, __ATOMIC_RELAXED);
	} else {
		HA_IPC_LOG(INFO, "Connected to ipc server, starting HA mode");
		__atomic_store_n(&ipc_client_connected, true, __ATOMIC_RELAXED);
	}

	if (pthread_create(&thread, NULL, &ipc_connection_handler, NULL) != 0) {
		HA_IPC_LOG(ERR, "Failed to create ipc conn handler");
		return -1;
	}

	return 0;
}

int
virtio_ha_pf_list_query(struct virtio_dev_name **list)
{
	struct virtio_ha_msg *msg;
	uint32_t nr_pf;
	int ret;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		return -1;
	}

	msg->hdr.type = VIRTIO_HA_APP_QUERY_PF_LIST;
	ret = virtio_ha_send_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to send msg");
		return -1;
	}

	ret = virtio_ha_recv_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to recv msg");
		return -1;
	}

	*list = (struct virtio_dev_name *)msg->iov.iov_base;
	nr_pf = msg->iov.iov_len / sizeof(struct virtio_dev_name);

	virtio_ha_free_msg(msg);

	return nr_pf;
}

int
virtio_ha_vf_list_query(const struct virtio_dev_name *pf,
    struct vdpa_vf_with_devargs **vf_list)
{
	struct virtio_ha_msg *msg;
	uint32_t nr_vf;
	int ret;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		return -1;
	}

	msg->hdr.type = VIRTIO_HA_APP_QUERY_VF_LIST;
	memcpy(msg->hdr.bdf, pf->dev_bdf, PCI_PRI_STR_SIZE);
	ret = virtio_ha_send_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to send msg");
		return -1;
	}

	ret = virtio_ha_recv_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to recv msg");
		return -1;
	}

	*vf_list = (struct vdpa_vf_with_devargs *)msg->iov.iov_base;
	nr_vf = msg->iov.iov_len / sizeof(struct vdpa_vf_with_devargs);

	virtio_ha_free_msg(msg);

	return nr_vf;
}

int
virtio_ha_pf_ctx_query(const struct virtio_dev_name *pf, struct virtio_pf_ctx *ctx)
{
	struct virtio_ha_msg *msg;
	int ret;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		return -1;
	}

	msg->hdr.type = VIRTIO_HA_APP_QUERY_PF_CTX;
	memcpy(msg->hdr.bdf, pf->dev_bdf, PCI_PRI_STR_SIZE);
	ret = virtio_ha_send_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to send msg");
		return -1;
	}

	ret = virtio_ha_recv_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to recv msg");
		return -1;
	}

	if (msg->nr_fds != 2) {
		HA_IPC_LOG(ERR, "Wrong number of fds");
		return -1;
	}

	ctx->vfio_group_fd = msg->fds[0];
	ctx->vfio_device_fd = msg->fds[1];

	virtio_ha_free_msg(msg);

	return 0;
}

int
virtio_ha_vf_ctx_query(struct virtio_dev_name *vf,
	const struct virtio_dev_name *pf, struct vdpa_vf_ctx **ctx)
{
	struct virtio_vdpa_dma_mem *mem;
	struct virtio_ha_msg *msg;
	int ret;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		return -1;
	}

	msg->hdr.type = VIRTIO_HA_APP_QUERY_VF_CTX;
	msg->hdr.size = sizeof(struct virtio_dev_name);
	memcpy(msg->hdr.bdf, pf->dev_bdf, PCI_PRI_STR_SIZE);
	msg->iov.iov_len = sizeof(struct virtio_dev_name);
	msg->iov.iov_base = vf;
	ret = virtio_ha_send_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to send msg");
		return -1;
	}

	ret = virtio_ha_recv_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to recv msg");
		return -1;
	}

	if (msg->nr_fds != 3) {
		HA_IPC_LOG(ERR, "Wrong number of fds");
		return -1;
	}

	mem = (struct virtio_vdpa_dma_mem *)msg->iov.iov_base;
	*ctx = malloc(sizeof(struct vdpa_vf_ctx) +
		sizeof(struct virtio_vdpa_mem_region) * mem->nregions);
	if (*ctx == NULL) {
		HA_IPC_LOG(ERR, "Failed to alloc vf ctx");
		return -1;			
	}

	(*ctx)->vfio_container_fd = msg->fds[0];
	(*ctx)->vfio_group_fd = msg->fds[1];
	(*ctx)->vfio_device_fd = msg->fds[2];
	memcpy(&(*ctx)->mem, mem, msg->iov.iov_len);
	free(msg->iov.iov_base);//TO-DO: should make ctx->mem a pointer?

	virtio_ha_free_msg(msg);

	return 0;
}

int
virtio_ha_pf_ctx_store(const struct virtio_dev_name *pf, const struct virtio_pf_ctx *ctx)
{
	struct virtio_ha_msg *msg;
	int ret;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		return -1;
	}

	msg->hdr.type = VIRTIO_HA_PF_STORE_CTX;
	memcpy(msg->hdr.bdf, pf->dev_bdf, PCI_PRI_STR_SIZE);
	msg->nr_fds = 2;
	msg->fds[0] = ctx->vfio_group_fd;
	msg->fds[1] = ctx->vfio_device_fd;
	ret = virtio_ha_send_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to send msg");
		return -1;
	}

	virtio_ha_free_msg(msg);

	return 0;
}

int
virtio_ha_pf_ctx_remove(const struct virtio_dev_name *pf)
{
	struct virtio_ha_msg *msg;
	int ret;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		return -1;
	}

	msg->hdr.type = VIRTIO_HA_PF_REMOVE_CTX;
	memcpy(msg->hdr.bdf, pf->dev_bdf, PCI_PRI_STR_SIZE);
	ret = virtio_ha_send_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to send msg");
		return -1;
	}

	virtio_ha_free_msg(msg);

	return 0;
}

int
virtio_ha_vf_devargs_fds_store(struct vdpa_vf_with_devargs *vf_dev,
    const struct virtio_dev_name *pf, int vfio_container_fd,
	int vfio_group_fd, int vfio_device_fd)
{
	struct virtio_ha_msg *msg;
	int ret;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		return -1;
	}

	msg->hdr.type = VIRTIO_HA_VF_STORE_DEVARG_VFIO_FDS;
	msg->hdr.size = sizeof(struct vdpa_vf_with_devargs);
	memcpy(msg->hdr.bdf, pf->dev_bdf, PCI_PRI_STR_SIZE);
	msg->nr_fds = 3;
	msg->fds[0] = vfio_container_fd;
	msg->fds[1] = vfio_group_fd;
	msg->fds[2] = vfio_device_fd;
	msg->iov.iov_len = sizeof(struct vdpa_vf_with_devargs);
	msg->iov.iov_base = vf_dev;
	ret = virtio_ha_send_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to send msg");
		return -1;
	}

	virtio_ha_free_msg(msg);

	return 0;
}

int
virtio_ha_vf_devargs_fds_remove(struct virtio_dev_name *vf,
	const struct virtio_dev_name *pf)
{
	struct virtio_ha_msg *msg;
	int ret;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		return -1;
	}

	msg->hdr.type = VIRTIO_HA_VF_REMOVE_DEVARG_VFIO_FDS;
	msg->hdr.size = sizeof(struct virtio_dev_name);
	memcpy(msg->hdr.bdf, pf->dev_bdf, PCI_PRI_STR_SIZE);
	msg->iov.iov_len = sizeof(struct virtio_dev_name);
	msg->iov.iov_base = vf;
	ret = virtio_ha_send_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to send msg");
		return -1;
	}

	virtio_ha_free_msg(msg);

	return 0;
}

int
virtio_ha_vf_vhost_fd_store(struct virtio_dev_name *vf,
	const struct virtio_dev_name *pf, int fd)
{
	struct virtio_ha_msg *msg;
	int ret;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		return -1;
	}

	msg->hdr.type = VIRTIO_HA_VF_STORE_VHOST_FD;
	msg->hdr.size = sizeof(struct virtio_dev_name);
	memcpy(msg->hdr.bdf, pf->dev_bdf, PCI_PRI_STR_SIZE);
	msg->nr_fds = 1;
	msg->fds[0] = fd;
	msg->iov.iov_len = sizeof(struct virtio_dev_name);
	msg->iov.iov_base = vf;
	ret = virtio_ha_send_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to send msg");
		return -1;
	}

	virtio_ha_free_msg(msg);

	return 0;	
}

int
virtio_ha_vf_vhost_fd_remove(struct virtio_dev_name *vf,
	const struct virtio_dev_name *pf)
{
	struct virtio_ha_msg *msg;
	int ret;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		return -1;
	}

	msg->hdr.type = VIRTIO_HA_VF_REMOVE_VHOST_FD;
	msg->hdr.size = sizeof(struct virtio_dev_name);
	memcpy(msg->hdr.bdf, pf->dev_bdf, PCI_PRI_STR_SIZE);
	msg->iov.iov_len = sizeof(struct virtio_dev_name);
	msg->iov.iov_base = vf;
	ret = virtio_ha_send_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to send msg");
		return -1;
	}

	virtio_ha_free_msg(msg);

	return 0;
}

int
virtio_ha_vf_mem_tbl_store(const struct virtio_dev_name *vf,
    const struct virtio_dev_name *pf, const struct virtio_vdpa_dma_mem *mem)
{
	struct virtio_dev_name *vf_dev;
	struct virtio_ha_msg *msg;
	size_t mem_len;
	int ret;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		return -1;
	}

	msg->hdr.type = VIRTIO_HA_VF_STORE_DMA_TBL;
	memcpy(msg->hdr.bdf, pf->dev_bdf, PCI_PRI_STR_SIZE);
	mem_len = sizeof(struct virtio_vdpa_dma_mem) +
		mem->nregions * sizeof(struct virtio_vdpa_mem_region);
	msg->hdr.size = sizeof(struct virtio_dev_name) + mem_len;
	msg->iov.iov_len = sizeof(struct virtio_dev_name) + mem_len;
	msg->iov.iov_base = malloc(msg->iov.iov_len);
	if (msg->iov.iov_base == NULL) {
		HA_IPC_LOG(ERR, "Failed to alloc iov base");
		return -1;			
	}
	vf_dev = (struct virtio_dev_name *)msg->iov.iov_base;
	memcpy(vf_dev, vf, sizeof(struct virtio_dev_name));
	memcpy(vf_dev + 1, mem, mem_len);
	ret = virtio_ha_send_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to send msg");
		return -1;
	}

	free(msg->iov.iov_base);
	virtio_ha_free_msg(msg);

	return 0;	
}

int
virtio_ha_vf_mem_tbl_remove(struct virtio_dev_name *vf,
	const struct virtio_dev_name *pf)
{
	struct virtio_ha_msg *msg;
	int ret;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		return -1;
	}

	msg->hdr.type = VIRTIO_HA_VF_REMOVE_DMA_TBL;
	msg->hdr.size = sizeof(struct virtio_dev_name);
	memcpy(msg->hdr.bdf, pf->dev_bdf, PCI_PRI_STR_SIZE);
	msg->iov.iov_len = sizeof(struct virtio_dev_name);
	msg->iov.iov_base = vf;
	ret = virtio_ha_send_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to send msg");
		return -1;
	}

	virtio_ha_free_msg(msg);

	return 0;	
}
