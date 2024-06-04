/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <unistd.h>
#include <pthread.h>

#include <rte_log.h>
#include <rte_spinlock.h>

#include "virtio_ha.h"

RTE_LOG_REGISTER(virtio_ha_ipc_logtype, pmd.vdpa.virtio, NOTICE);
#define HA_IPC_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, virtio_ha_ipc_logtype, \
		"VIRTIO HA IPC %s(): " fmt "\n", __func__, ##args)

static int ipc_client_sock;
static bool ipc_client_connected;
static bool ipc_client_sync;
static const struct virtio_ha_dev_ctx_cb *pf_ctx_cb;
static const struct virtio_ha_dev_ctx_cb *vf_ctx_cb;
static struct virtio_ha_device_list client_devs;
static struct virtio_ha_msg *prio_msg;
struct virtio_ha_version *ha_ver;

static void sync_dev_context_to_ha(ver_time_set set_ver);

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
		HA_IPC_LOG(ERR, "Failed to send complete msg (sz %d instead of %lu fd %d)",
			ret, msg_sz, sockfd);
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
		HA_IPC_LOG(ERR, "Unexpected header size read (%d instead of %lu)", ret, sizeof(*hdr));
		ret = -1;
		goto out;
	}

	if (hdr->size) {
		msg->iov.iov_len = hdr->size;
		msg->iov.iov_base = malloc(msg->iov.iov_len);
		if (msg->iov.iov_base == NULL) {
			HA_IPC_LOG(ERR, "Failed to alloc message payload (sz %u) when read message", hdr->size);
			ret = -1;
			goto out;
		}
		ret = read(sockfd, msg->iov.iov_base, msg->iov.iov_len);
		if (ret <= 0)
			goto out;
		if (ret != (int)msg->iov.iov_len) {
			HA_IPC_LOG(ERR, "Failed to read complete message payload (%d instead of %lu,fd %d)",
				ret, msg->iov.iov_len, sockfd);
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
ipc_connection_handler(void *ptr)
{
	struct epoll_event event, ev;
	struct sockaddr_un addr;
	ver_time_set set_ver;
	int epfd, nev;

	set_ver = (ver_time_set)ptr;

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
					set_ver(NULL, NULL);

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
				__atomic_store_n(&ipc_client_sync, true, __ATOMIC_RELAXED);
				sync_dev_context_to_ha(set_ver);
				__atomic_store_n(&ipc_client_sync, false, __ATOMIC_RELAXED);
			} else {
				sleep(1);
			}
		}
    }
}

static int
virtio_ha_version_query(struct virtio_ha_version *ver)
{
	struct virtio_ha_msg *msg;
	int ret;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		return -1;
	}

	msg->hdr.type = VIRTIO_HA_APP_QUERY_VERSION;
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

	if (msg->iov.iov_len != sizeof(struct virtio_ha_version)) {
		HA_IPC_LOG(ERR, "Wrong iov len");
		return -1;		
	}

	memcpy(ver, msg->iov.iov_base, sizeof(struct virtio_ha_version));
	free(msg->iov.iov_base);
	virtio_ha_free_msg(msg);

	return 0;
}

int
virtio_ha_ipc_client_init(ver_time_set set_ver)
{
	struct sockaddr_un addr;
	pthread_t thread;

	TAILQ_INIT(&client_devs.pf_list);
	TAILQ_INIT(&client_devs.dma_tbl);
	client_devs.nr_pf = 0;
	client_devs.global_cfd = -1;
	client_devs.prio_chnl_fd = -1;

	ha_ver = malloc(sizeof(struct virtio_ha_version));
	if (!ha_ver) {
		HA_IPC_LOG(ERR, "Failed to alloc ha version");
		return -1;
	}

	ipc_client_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ipc_client_sock < 0) {
		HA_IPC_LOG(ERR, "Failed to create ipc client socket");
		goto err;
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, VIRTIO_HA_UDS_PATH);
	if (connect(ipc_client_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		HA_IPC_LOG(INFO, "Failed to connect to ipc server, starting non-HA mode");
		__atomic_store_n(&ipc_client_connected, false, __ATOMIC_RELAXED);
		set_ver(NULL, NULL);
	} else {
		HA_IPC_LOG(INFO, "Connected to ipc server, starting HA mode");
		__atomic_store_n(&ipc_client_connected, true, __ATOMIC_RELAXED);
		if (virtio_ha_version_query(ha_ver)) {
			HA_IPC_LOG(ERR, "Failed to query HA version");
			goto err_sock;
		}
		set_ver(ha_ver->version, ha_ver->time);		
	}

	__atomic_store_n(&ipc_client_sync, false, __ATOMIC_RELAXED);

	if (pthread_create(&thread, NULL, &ipc_connection_handler, set_ver) != 0) {
		HA_IPC_LOG(ERR, "Failed to create ipc conn handler");
		goto err_sock;
	}

	return 0;

err_sock:
	close(ipc_client_sock);
err:
	free(ha_ver);
	return -1;
}

static void
prio_chnl_message_handler(int fd, void *data)
{
	struct virtio_ha_vf_restore_queue *rq = (struct virtio_ha_vf_restore_queue *)data;
	struct virtio_ha_vf_to_restore *vf_dev;
	struct virtio_dev_name *vf;
	bool found = false;
	int ret;

	virtio_ha_reset_msg(prio_msg);

	ret = virtio_ha_recv_msg(fd, prio_msg);
	if (ret <= 0) {
		if (ret < 0)
			HA_IPC_LOG(ERR, "Failed to recv ha priority msg");
		else
			HA_IPC_LOG(ERR, "Client closed");
		return;
	}

	if (prio_msg->hdr.type != VIRTIO_HA_PRIO_CHNL_ADD_VF) {
		HA_IPC_LOG(ERR, "Received wrong msg %d on priority channel", prio_msg->hdr.type);
		goto err;
	}

	if (prio_msg->hdr.size != sizeof(struct virtio_dev_name) ||
		prio_msg->iov.iov_len != sizeof(struct virtio_dev_name)) {
		HA_IPC_LOG(ERR, "Received wrong msg len(hdr_size %u, iov_len %lu) on priority channel",
			prio_msg->hdr.size, prio_msg->iov.iov_len);
		goto err;
	}

	vf = (struct virtio_dev_name *)prio_msg->iov.iov_base;
	pthread_mutex_lock(&rq->lock);
	TAILQ_FOREACH(vf_dev, &rq->non_prio_q, next) {
		if (!strcmp(vf_dev->vf_devargs.vf_name.dev_bdf, vf->dev_bdf)) {
			found = true;
			break;
		}
	}

	if (!found)
		goto unlock;

	TAILQ_REMOVE(&rq->non_prio_q, vf_dev, next);
	TAILQ_INSERT_TAIL(&rq->prio_q, vf_dev, next);
	HA_IPC_LOG(INFO, "Add vf %s to priority queue", vf_dev->vf_devargs.vf_name.dev_bdf);

unlock:
	pthread_mutex_unlock(&rq->lock);
err:
	free(prio_msg->iov.iov_base);
	return;	
}

static void *
prio_chnl_thread(void *arg)
{
	struct epoll_event event, ev;
	struct virtio_ha_event_handler hdl, *handler;
	int epfd, nev;

	epfd = epoll_create(1);
	if (epfd < 0) {
		HA_IPC_LOG(ERR, "Failed to create epoll fd");
		return NULL;
	}

	hdl.sock = client_devs.prio_chnl_fd;
	hdl.cb = prio_chnl_message_handler;
	hdl.data = arg;
	event.events = EPOLLIN | EPOLLHUP | EPOLLERR;
	event.data.ptr = &hdl;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, hdl.sock, &event) < 0) {
		HA_IPC_LOG(ERR, "Failed to epoll ctl add for priority channel");
		return NULL;
	}

	while (1) {
		nev = epoll_wait(epfd, &ev, 1, -1);
		if (nev > 0) {
			handler = (struct virtio_ha_event_handler *)ev.data.ptr;
			if ((ev.events & EPOLLERR) || (ev.events & EPOLLHUP)) {
				if (epoll_ctl(epfd, EPOLL_CTL_DEL, handler->sock, &ev) < 0)
					HA_IPC_LOG(ERR, "Failed to epoll ctl del for fd %d", handler->sock);
				close(handler->sock);
				close(epfd);
				break;
			} else { /* EPOLLIN */
				handler->cb(handler->sock, handler->data);
			}
		}
	}

	return NULL;
}

int
virtio_ha_prio_chnl_init(struct virtio_ha_vf_restore_queue *rq)
{
	struct virtio_ha_msg *msg;
	int ret, sv[2];

	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;

	ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
	if (ret) {
		HA_IPC_LOG(ERR, "Failed to create socket pair");
		return -1;
	}

	client_devs.prio_chnl_fd = sv[0];

	prio_msg = virtio_ha_alloc_msg();
	if (!prio_msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ha priority msg");
		ret = -1;
		goto err;
	}

	ret = pthread_create(&client_devs.prio_thread, NULL, prio_chnl_thread, (void *)rq);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to create priority thread");
		ret = -1;
		goto err;
	}

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		ret = -1;
		goto err;
	}

	msg->hdr.type = VIRTIO_HA_APP_SET_PRIO_CHNL;
	msg->nr_fds = 1;
	msg->fds[0] = sv[1];
	ret = virtio_ha_send_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to send msg");
		goto err;
	}

	virtio_ha_free_msg(msg);
	close(sv[1]);

	return 0;

err:
	close(sv[0]);
	close(sv[1]);
	return ret;
}

void
virtio_ha_prio_chnl_destroy(void)
{
	struct virtio_ha_msg *msg;
	int ret;

	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

	if (client_devs.prio_chnl_fd != -1)
		close(client_devs.prio_chnl_fd);

	if (!prio_msg)
		virtio_ha_free_msg(prio_msg);

	pthread_cancel(client_devs.prio_thread);
	pthread_join(client_devs.prio_thread, NULL);

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		return;
	}

	msg->hdr.type = VIRTIO_HA_APP_REMOVE_PRIO_CHNL;
	ret = virtio_ha_send_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to send msg");
		return;
	}

	virtio_ha_free_msg(msg);

	return;
}

static struct virtio_ha_pf_dev *
alloc_pf_dev_to_list(const struct virtio_dev_name *pf)
{
	struct virtio_ha_pf_dev *dev;

	dev = malloc(sizeof(struct virtio_ha_pf_dev));
	if (dev == NULL) {
		HA_IPC_LOG(ERR, "Failed to alloc pf dev");
		return NULL;
	}

	memset(dev, 0, sizeof(struct virtio_ha_pf_dev));
	TAILQ_INIT(&dev->vf_list);
	dev->nr_vf = 0;
	memcpy(dev->pf_name.dev_bdf, pf->dev_bdf, PCI_PRI_STR_SIZE);
	TAILQ_INSERT_TAIL(&client_devs.pf_list, dev, next);

	return dev;
}

static struct virtio_ha_vf_dev *
alloc_vf_dev_to_list(struct vdpa_vf_with_devargs *vf_dev, const struct virtio_dev_name *pf)
{
	struct virtio_ha_vf_dev_list *vf_list = NULL;
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf;
	size_t len;
	bool found = false;

	TAILQ_FOREACH(dev, &client_devs.pf_list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, pf->dev_bdf)) {
			vf_list = &dev->vf_list;
			found = true;
			break;
		}
	}

	if (!found)
		return NULL;

	/* To avoid memory realloc when mem table entry number changes, alloc for max entry num */
	len = sizeof(struct virtio_ha_vf_dev) +
		sizeof(struct virtio_vdpa_mem_region) * VIRTIO_HA_MAX_MEM_REGIONS;
	vf = malloc(len);
	if (vf == NULL) {
		HA_IPC_LOG(ERR, "Failed to alloc vf device");
		return NULL;
	}

	memset(vf, 0, len);
	memcpy(&vf->vf_devargs, vf_dev, sizeof(struct vdpa_vf_with_devargs));
	TAILQ_INSERT_TAIL(vf_list, vf, next);

	return vf;
}

int
virtio_ha_pf_list_query(struct virtio_dev_name **list)
{
	struct virtio_ha_msg *msg;
	struct virtio_ha_pf_dev *dev;
	struct virtio_dev_name *names;
	uint32_t nr_pf, i;
	int ret;

	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

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

	names = (struct virtio_dev_name *)msg->iov.iov_base;
	*list = names;
	nr_pf = msg->iov.iov_len / sizeof(struct virtio_dev_name);

	virtio_ha_free_msg(msg);

	for (i = 0; i < nr_pf; i++) {
		dev = alloc_pf_dev_to_list(names + i);
		if (!dev) {
			HA_IPC_LOG(ERR, "Failed to alloc pf %s to list", names[i].dev_bdf);
			return nr_pf;			
		}
		dev->pf_ctx.vfio_device_fd = -1;
		dev->pf_ctx.vfio_group_fd = -1;
	}

	return nr_pf;
}

int
virtio_ha_vf_list_query(const struct virtio_dev_name *pf,
    struct vdpa_vf_with_devargs **vf_list)
{
	struct virtio_ha_vf_dev *vf;
	struct virtio_ha_msg *msg;
	struct vdpa_vf_with_devargs *dev;
	uint32_t nr_vf, i;
	int ret;

	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

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

	dev = (struct vdpa_vf_with_devargs *)msg->iov.iov_base;
	*vf_list = dev;
	nr_vf = msg->iov.iov_len / sizeof(struct vdpa_vf_with_devargs);

	virtio_ha_free_msg(msg);

	for (i = 0; i < nr_vf; i++) {
		vf = alloc_vf_dev_to_list(dev + i, pf);
		if (!vf) {
			HA_IPC_LOG(ERR, "Failed to alloc vf %s to list", dev[i].vf_name.dev_bdf);
			return nr_vf;
		}
		vf->vf_ctx.vfio_container_fd = -1;
		vf->vf_ctx.vfio_group_fd = -1;
		vf->vf_ctx.vfio_device_fd = -1;
		vf->vhost_fd = -1;
	}

	return nr_vf;
}

int
virtio_ha_pf_ctx_query(const struct virtio_dev_name *pf, struct virtio_pf_ctx *ctx)
{
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_msg *msg;
	int ret;

	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

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

	TAILQ_FOREACH(dev, &client_devs.pf_list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, pf->dev_bdf)) {
			dev->pf_ctx.vfio_group_fd = msg->fds[0];
			dev->pf_ctx.vfio_device_fd = msg->fds[1];
			break;
		}
	}

	virtio_ha_free_msg(msg);

	return 0;
}

int
virtio_ha_vf_ctx_query(struct virtio_dev_name *vf,
	const struct virtio_dev_name *pf, struct vdpa_vf_ctx **ctx)
{
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf_dev;
	struct virtio_ha_vf_dev_list *vf_list;
	struct virtio_ha_msg *msg;
	struct vdpa_vf_ctx_content *ctt;
	bool found = false;
	int ret;

	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return -2;

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

	ctt = (struct vdpa_vf_ctx_content *)msg->iov.iov_base;

	*ctx = malloc(sizeof(struct vdpa_vf_ctx) +
		ctt->mem.nregions * sizeof(struct virtio_vdpa_mem_region));
	if (*ctx == NULL) {
		HA_IPC_LOG(ERR, "Failed to alloc vf ctx");
		return -1;			
	}

	(*ctx)->vfio_container_fd = msg->fds[0];
	(*ctx)->vfio_group_fd = msg->fds[1];
	(*ctx)->vfio_device_fd = msg->fds[2];
	memcpy(&(*ctx)->ctt, msg->iov.iov_base, msg->iov.iov_len);

	TAILQ_FOREACH(dev, &client_devs.pf_list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, pf->dev_bdf)) {
			vf_list = &dev->vf_list;
			found = true;
			break;
		}
	}

	if (!found)
		return 0;

	TAILQ_FOREACH(vf_dev, vf_list, next) {
		if (!strcmp(vf_dev->vf_devargs.vf_name.dev_bdf, vf->dev_bdf)) {
			vf_dev->vf_ctx.vfio_container_fd = msg->fds[0];
			vf_dev->vf_ctx.vfio_group_fd = msg->fds[1];
			vf_dev->vf_ctx.vfio_device_fd = msg->fds[2];
			memcpy(&vf_dev->vf_ctx.ctt, msg->iov.iov_base, msg->iov.iov_len);
			break;
		}
	}

	free(msg->iov.iov_base);
	virtio_ha_free_msg(msg);

	return 0;
}

int
virtio_ha_pf_ctx_set(const struct virtio_dev_name *pf, const struct virtio_pf_ctx *ctx)
{
	if (!pf_ctx_cb || !pf_ctx_cb->set)
		return -1;

	pf_ctx_cb->set(pf, ctx);

	return 0;
}

int
virtio_ha_pf_ctx_unset(const struct virtio_dev_name *pf)
{
	if (!pf_ctx_cb || !pf_ctx_cb->unset)
		return -1;

	pf_ctx_cb->unset(pf);

	return 0;
}

int
virtio_ha_vf_ctx_set(const struct virtio_dev_name *vf, const struct vdpa_vf_ctx *ctx)
{
	if (!vf_ctx_cb || !vf_ctx_cb->set)
		return -1;

	vf_ctx_cb->set(vf, ctx);

	return 0;
}

int
virtio_ha_vf_ctx_unset(const struct virtio_dev_name *vf)
{
	if (!vf_ctx_cb || !vf_ctx_cb->unset)
		return -1;

	vf_ctx_cb->unset(vf);

	return 0;
}

void
virtio_ha_pf_register_ctx_cb(const struct virtio_ha_dev_ctx_cb *ctx_cb)
{
	pf_ctx_cb = ctx_cb;
	return;
}

static int
virtio_ha_pf_ctx_store_no_cache(const struct virtio_dev_name *pf, const struct virtio_pf_ctx *ctx)
{
	struct virtio_ha_msg *msg;
	int ret;

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
virtio_ha_pf_ctx_store(const struct virtio_dev_name *pf, const struct virtio_pf_ctx *ctx)
{
	struct virtio_ha_pf_dev *dev;

	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

	dev = alloc_pf_dev_to_list(pf);
	if (!dev) {
		HA_IPC_LOG(ERR, "Alloc pf %s to list failed", pf->dev_bdf);
		return -1;
	}

	dev->pf_ctx.vfio_group_fd = ctx->vfio_group_fd;
	dev->pf_ctx.vfio_device_fd = ctx->vfio_device_fd;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;
	else
		return virtio_ha_pf_ctx_store_no_cache(pf, ctx);
}

int
virtio_ha_pf_ctx_remove(const struct virtio_dev_name *pf)
{
	struct virtio_ha_vf_dev_list *vf_list = NULL;
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf_dev;
	bool found = false;

	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

	TAILQ_FOREACH(dev, &client_devs.pf_list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, pf->dev_bdf)) {
			vf_list = &dev->vf_list;
			found = true;
			break;
		}
	}

	if (!found)
		return -1;

	if (vf_list) {
		TAILQ_FOREACH(vf_dev, vf_list, next)
			free(vf_dev);
	}

	TAILQ_REMOVE(&client_devs.pf_list, dev, next);
	free(dev);

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED)) {
		return 0;
	} else {
		struct virtio_ha_msg *msg;
		int ret;

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
}

void
virtio_ha_vf_register_ctx_cb(const struct virtio_ha_dev_ctx_cb *ctx_cb)
{
	vf_ctx_cb = ctx_cb;
	return;
}

static int
virtio_ha_vf_devargs_fds_store_no_cache(struct vdpa_vf_with_devargs *vf_dev,
    const struct virtio_dev_name *pf, int vfio_container_fd,
	int vfio_group_fd, int vfio_device_fd)
{
	struct virtio_ha_msg *msg;
	int ret;

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
virtio_ha_vf_devargs_fds_store(struct vdpa_vf_with_devargs *vf_dev,
    const struct virtio_dev_name *pf, int vfio_container_fd,
	int vfio_group_fd, int vfio_device_fd)
{
	struct virtio_ha_vf_dev *vf;

	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

	vf = alloc_vf_dev_to_list(vf_dev, pf);
	if (!vf) {
		HA_IPC_LOG(ERR, "Failed to alloc vf %s to list", vf_dev->vf_name.dev_bdf);
		return -1;
	}

	vf->vf_ctx.vfio_container_fd = vfio_container_fd;
	vf->vf_ctx.vfio_group_fd = vfio_group_fd;
	vf->vf_ctx.vfio_device_fd = vfio_device_fd;
	vf->vhost_fd = -1;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;
	else
		return virtio_ha_vf_devargs_fds_store_no_cache(vf_dev, pf, vfio_container_fd,
			vfio_group_fd, vfio_device_fd);
}

int
virtio_ha_vf_devargs_fds_remove(struct virtio_dev_name *vf,
	const struct virtio_dev_name *pf)
{
	struct virtio_ha_vf_dev_list *vf_list = NULL;
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf_dev;
	bool found = false;

	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

	TAILQ_FOREACH(dev, &client_devs.pf_list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, pf->dev_bdf)) {
			vf_list = &dev->vf_list;
			found = true;
			break;
		}
	}

	if (!found)
		return -1;

	found = false;
	TAILQ_FOREACH(vf_dev, vf_list, next) {
		if (!strcmp(vf_dev->vf_devargs.vf_name.dev_bdf, vf->dev_bdf)) {
			found = true;
			break;
		}
	}

	if (found) {
		TAILQ_REMOVE(vf_list, vf_dev, next);
		free(vf_dev);
	}

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED)) {
		return 0;
	} else {
		struct virtio_ha_msg *msg;
		int ret;

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
}

static int
virtio_ha_vf_vhost_fd_store_no_cache(struct virtio_dev_name *vf,
	const struct virtio_dev_name *pf, int fd)
{
	struct virtio_ha_msg *msg;
	int ret;

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
virtio_ha_vf_vhost_fd_store(struct virtio_dev_name *vf,
	const struct virtio_dev_name *pf, int fd)
{
	struct virtio_ha_vf_dev_list *vf_list = NULL;
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf_dev;
	bool found = false;

	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

	TAILQ_FOREACH(dev, &client_devs.pf_list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, pf->dev_bdf)) {
			vf_list = &dev->vf_list;
			found = true;
			break;
		}
	}

	if (!found)
		return -1;

	TAILQ_FOREACH(vf_dev, vf_list, next) {
		if (!strcmp(vf_dev->vf_devargs.vf_name.dev_bdf, vf->dev_bdf)) {
			vf_dev->vhost_fd = fd;
			break;
		}
	}

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;
	else 
		return virtio_ha_vf_vhost_fd_store_no_cache(vf, pf, fd);
}

int
virtio_ha_vf_vhost_fd_remove(struct virtio_dev_name *vf,
	const struct virtio_dev_name *pf)
{
	struct virtio_ha_vf_dev_list *vf_list = NULL;
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf_dev;
	bool found = false;

	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

	TAILQ_FOREACH(dev, &client_devs.pf_list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, pf->dev_bdf)) {
			vf_list = &dev->vf_list;
			found = true;
			break;
		}
	}

	if (!found)
		return -1;

	TAILQ_FOREACH(vf_dev, vf_list, next) {
		if (!strcmp(vf_dev->vf_devargs.vf_name.dev_bdf, vf->dev_bdf)) {
			vf_dev->vhost_fd = -1;
			break;
		}
	}

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED)) {
		return 0;
	} else {
		struct virtio_ha_msg *msg;
		int ret;

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
}

static int
virtio_ha_vf_mem_tbl_store_no_cache(const struct virtio_dev_name *vf,
    const struct virtio_dev_name *pf, const struct virtio_vdpa_dma_mem *mem)
{
	struct virtio_dev_name *vf_dev;
	struct virtio_ha_msg *msg;
	size_t mem_len;
	int ret;

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
virtio_ha_vf_mem_tbl_store(const struct virtio_dev_name *vf,
    const struct virtio_dev_name *pf, const struct virtio_vdpa_dma_mem *mem)
{
	struct virtio_ha_vf_dev_list *vf_list = NULL;
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf_dev;
	size_t len;
	bool found = false;

	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

	TAILQ_FOREACH(dev, &client_devs.pf_list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, pf->dev_bdf)) {
			vf_list = &dev->vf_list;
			found = true;
			break;
		}
	}

	if (!found)
		return -1;

	len = sizeof(struct virtio_vdpa_dma_mem) +
		mem->nregions * sizeof(struct virtio_vdpa_mem_region);
	TAILQ_FOREACH(vf_dev, vf_list, next) {
		if (!strcmp(vf_dev->vf_devargs.vf_name.dev_bdf, vf->dev_bdf)) {
			memcpy(&vf_dev->vf_ctx.ctt.mem, mem, len);
			break;
		}
	}

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;
	else
		return virtio_ha_vf_mem_tbl_store_no_cache(vf, pf, mem);
}

int
virtio_ha_vf_mem_tbl_remove(struct virtio_dev_name *vf,
	const struct virtio_dev_name *pf)
{
	struct virtio_ha_vf_dev_list *vf_list = NULL;
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf_dev;
	struct virtio_vdpa_dma_mem *mem;
	bool found = false;

	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

	TAILQ_FOREACH(dev, &client_devs.pf_list, next) {
		if (!strcmp(dev->pf_name.dev_bdf, pf->dev_bdf)) {
			vf_list = &dev->vf_list;
			found = true;
			break;
		}
	}

	if (!found)
		return -1;

	TAILQ_FOREACH(vf_dev, vf_list, next) {
		if (!strcmp(vf_dev->vf_devargs.vf_name.dev_bdf, vf->dev_bdf)) {
			mem = &vf_dev->vf_ctx.ctt.mem;
			mem->nregions = 0;
			break;
		}
	}

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED)) {
		return 0;
	} else {
		struct virtio_ha_msg *msg;
		int ret;

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
}

static int
virtio_ha_global_cfd_store_no_cache(int vfio_container_fd)
{
	struct virtio_ha_msg *msg;
	int ret;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		return -1;
	}

	msg->hdr.type = VIRTIO_HA_GLOBAL_STORE_CONTAINER;
	msg->fds[0] = vfio_container_fd;
	msg->nr_fds = 1;
	ret = virtio_ha_send_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to send msg");
		return -1;
	}

	virtio_ha_free_msg(msg);

	return 0;
}

int
virtio_ha_global_cfd_store(int vfio_container_fd)
{
	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

	client_devs.global_cfd = vfio_container_fd;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;
	else
		return virtio_ha_global_cfd_store_no_cache(vfio_container_fd);
}

int
virtio_ha_global_cfd_query(int *vfio_container_fd)
{
	struct virtio_ha_msg *msg;
	int ret;

	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		return -1;
	}

	msg->hdr.type = VIRTIO_HA_GLOBAL_QUERY_CONTAINER;
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

	if (msg->nr_fds != 1)
		goto out;

	*vfio_container_fd = msg->fds[0];
	client_devs.global_cfd = msg->fds[0];

out:
	virtio_ha_free_msg(msg);

	return 0;
}

static int
virtio_ha_global_dma_map_no_cache(struct virtio_ha_global_dma_map *map, bool is_map)
{
	struct virtio_ha_msg *msg;
	int ret;

	msg = virtio_ha_alloc_msg();
	if (!msg) {
		HA_IPC_LOG(ERR, "Failed to alloc ipc client msg");
		return -1;
	}

	msg->hdr.type = is_map ? VIRTIO_HA_GLOBAL_STORE_DMA_MAP : VIRTIO_HA_GLOBAL_REMOVE_DMA_MAP;
	msg->hdr.size = sizeof(struct virtio_ha_global_dma_map);
	msg->iov.iov_len = sizeof(struct virtio_ha_global_dma_map);
	msg->iov.iov_base = (void *)map;
	ret = virtio_ha_send_msg(ipc_client_sock, msg);
	if (ret < 0) {
		HA_IPC_LOG(ERR, "Failed to send msg");
		return -1;
	}

	virtio_ha_free_msg(msg);

	return 0;
}

int
virtio_ha_global_dma_map_store(struct virtio_ha_global_dma_map *map)
{
	struct virtio_ha_global_dma_entry *entry;
	bool found = false;

	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

	TAILQ_FOREACH(entry, &client_devs.dma_tbl, next) {
		if (map->iova == entry->map.iova) {
			found = true;
			break;
		}
	}

	if (!found) {
		entry = malloc(sizeof(struct virtio_ha_global_dma_entry));
		if (!entry) {
			HA_IPC_LOG(ERR, "Failed to alloc dma entry");
			return -1;
		}
		memcpy(&entry->map, map, sizeof(struct virtio_ha_global_dma_map));
		TAILQ_INSERT_TAIL(&client_devs.dma_tbl, entry, next);
	}

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED))
		return 0;
	else
		return virtio_ha_global_dma_map_no_cache(map, true);
}

int
virtio_ha_global_dma_map_remove(struct virtio_ha_global_dma_map *map)
{
	struct virtio_ha_global_dma_entry *entry;
	bool found = false;

	while (__atomic_load_n(&ipc_client_sync, __ATOMIC_RELAXED))
		;

	TAILQ_FOREACH(entry, &client_devs.dma_tbl, next) {
		if (map->iova == entry->map.iova) {
			found = true;
			break;
		}
	}

	if (found) {
		TAILQ_REMOVE(&client_devs.dma_tbl, entry, next);
		free(entry);
	}

	if (!__atomic_load_n(&ipc_client_connected, __ATOMIC_RELAXED)) 
		return 0;
	else
		return virtio_ha_global_dma_map_no_cache(map, false);
}

static void
sync_dev_context_to_ha(ver_time_set set_ver)
{
	struct virtio_ha_pf_dev *dev;
	struct virtio_ha_vf_dev *vf_dev;
	struct virtio_ha_vf_dev_list *vf_list;
	struct virtio_ha_global_dma_entry *entry;
	int ret;

	ret = virtio_ha_version_query(ha_ver);
	if (ret) {
		HA_IPC_LOG(ERR, "Failed to query version");
		return;
	}

	set_ver(ha_ver->version, ha_ver->time);

	ret = virtio_ha_global_cfd_store_no_cache(client_devs.global_cfd);
	if (ret) {
		HA_IPC_LOG(ERR, "Failed to sync global container fd");
		return;
	}

	TAILQ_FOREACH(entry, &client_devs.dma_tbl, next) {
		ret = virtio_ha_global_dma_map_no_cache(&entry->map, true);
		if (ret) {
			HA_IPC_LOG(ERR, "Failed to sync global dma map");
			return;
		}		
	}	

	TAILQ_FOREACH(dev, &client_devs.pf_list, next) {
		ret = virtio_ha_pf_ctx_store_no_cache(&dev->pf_name, &dev->pf_ctx);
		if (ret) {
			HA_IPC_LOG(ERR, "Failed to sync pf ctx");
			continue;
		}

		vf_list = &dev->vf_list;

		TAILQ_FOREACH(vf_dev, vf_list, next) {
			ret = virtio_ha_vf_devargs_fds_store_no_cache(&vf_dev->vf_devargs, &dev->pf_name,
				vf_dev->vf_ctx.vfio_container_fd, vf_dev->vf_ctx.vfio_group_fd,
				vf_dev->vf_ctx.vfio_device_fd);
			if (ret) {
				HA_IPC_LOG(ERR, "Failed to sync vf devargs and fds");
				continue;
			}

			if (vf_dev->vhost_fd != -1) {
				ret = virtio_ha_vf_vhost_fd_store_no_cache(&vf_dev->vf_devargs.vf_name,
						&dev->pf_name, vf_dev->vhost_fd);
				if (ret) {
					HA_IPC_LOG(ERR, "Failed to sync vf vhost fd");
					continue;
				}
			}

			if (vf_dev->vf_ctx.ctt.mem.nregions != 0) {
				ret = virtio_ha_vf_mem_tbl_store_no_cache(&vf_dev->vf_devargs.vf_name,
						&dev->pf_name, &vf_dev->vf_ctx.ctt.mem);
				if (ret) {
					HA_IPC_LOG(ERR, "Failed to sync vf memory table");
					continue;
				}
			}
		}
	}
}
