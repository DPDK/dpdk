/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <rte_log.h>

#include "fd_man.h"
#include "vhost.h"
#include "vhost_user.h"


TAILQ_HEAD(vhost_user_connection_list, vhost_user_connection);

/*
 * Every time rte_vhost_driver_register() is invoked, an associated
 * vhost_user_socket struct will be created.
 */
struct vhost_user_socket {
	struct vhost_user_connection_list conn_list;
	pthread_mutex_t conn_mutex;
	char *path;
	int socket_fd;
	struct sockaddr_un un;
	bool is_server;
	bool reconnect;
	bool dequeue_zero_copy;
	bool iommu_support;
	bool use_builtin_virtio_net;

	/*
	 * The "supported_features" indicates the feature bits the
	 * vhost driver supports. The "features" indicates the feature
	 * bits after the rte_vhost_driver_features_disable/enable().
	 * It is also the final feature bits used for vhost-user
	 * features negotiation.
	 */
	uint64_t supported_features;
	uint64_t features;

	uint64_t protocol_features;

	/*
	 * Device id to identify a specific backend device.
	 * It's set to -1 for the default software implementation.
	 * If valid, one socket can have 1 connection only.
	 */
	int vdpa_dev_id;

	struct vhost_device_ops const *notify_ops;
};

struct vhost_user_connection {
	struct vhost_user_socket *vsocket;
	int connfd;
	int vid;

	TAILQ_ENTRY(vhost_user_connection) next;
};

#define MAX_VHOST_SOCKET 1024
struct vhost_user {
	struct vhost_user_socket *vsockets[MAX_VHOST_SOCKET];
	struct fdset fdset;
	int vsocket_cnt;
	pthread_mutex_t mutex;
};

#define MAX_VIRTIO_BACKLOG 128

static void vhost_user_server_new_connection(int fd, void *data, int *remove);
static void vhost_user_read_cb(int fd, void *dat, int *remove);
static int create_unix_socket(struct vhost_user_socket *vsocket);
static int vhost_user_start_client(struct vhost_user_socket *vsocket);

static struct vhost_user vhost_user = {
	.fdset = {
		.fd = { [0 ... MAX_FDS - 1] = {-1, NULL, NULL, NULL, 0} },
		.fd_mutex = PTHREAD_MUTEX_INITIALIZER,
		.fd_pooling_mutex = PTHREAD_MUTEX_INITIALIZER,
		.num = 0
	},
	.vsocket_cnt = 0,
	.mutex = PTHREAD_MUTEX_INITIALIZER,
};

/*
 * return bytes# of read on success or negative val on failure. Update fdnum
 * with number of fds read.
 */
int
read_fd_message(int sockfd, char *buf, int buflen, int *fds, int max_fds,
		int *fd_num)
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
		RTE_LOG(ERR, VHOST_CONFIG, "recvmsg failed\n");
		return ret;
	}

	if (msgh.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) {
		RTE_LOG(ERR, VHOST_CONFIG, "truncted msg\n");
		return -1;
	}

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

int
send_fd_message(int sockfd, char *buf, int buflen, int *fds, int fd_num)
{

	struct iovec iov;
	struct msghdr msgh;
	size_t fdsize = fd_num * sizeof(int);
	char control[CMSG_SPACE(fdsize)];
	struct cmsghdr *cmsg;
	int ret;

	memset(&msgh, 0, sizeof(msgh));
	iov.iov_base = buf;
	iov.iov_len = buflen;

	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;

	if (fds && fd_num > 0) {
		msgh.msg_control = control;
		msgh.msg_controllen = sizeof(control);
		cmsg = CMSG_FIRSTHDR(&msgh);
		if (cmsg == NULL) {
			RTE_LOG(ERR, VHOST_CONFIG, "cmsg == NULL\n");
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
		RTE_LOG(ERR, VHOST_CONFIG,  "sendmsg error\n");
		return ret;
	}

	return ret;
}

static void
vhost_user_add_connection(int fd, struct vhost_user_socket *vsocket)
{
	int vid;
	size_t size;
	struct vhost_user_connection *conn;
	int ret;

	if (vsocket == NULL)
		return;

	conn = malloc(sizeof(*conn));
	if (conn == NULL) {
		close(fd);
		return;
	}

	vid = vhost_new_device();
	if (vid == -1) {
		goto err;
	}

	size = strnlen(vsocket->path, PATH_MAX);
	vhost_set_ifname(vid, vsocket->path, size);

	vhost_set_builtin_virtio_net(vid, vsocket->use_builtin_virtio_net);

	vhost_attach_vdpa_device(vid, vsocket->vdpa_dev_id);

	if (vsocket->dequeue_zero_copy)
		vhost_enable_dequeue_zero_copy(vid);

	RTE_LOG(INFO, VHOST_CONFIG, "new device, handle is %d\n", vid);

	if (vsocket->notify_ops->new_connection) {
		ret = vsocket->notify_ops->new_connection(vid);
		if (ret < 0) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"failed to add vhost user connection with fd %d\n",
				fd);
			goto err_cleanup;
		}
	}

	conn->connfd = fd;
	conn->vsocket = vsocket;
	conn->vid = vid;
	ret = fdset_add(&vhost_user.fdset, fd, vhost_user_read_cb,
			NULL, conn);
	if (ret < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"failed to add fd %d into vhost server fdset\n",
			fd);

		if (vsocket->notify_ops->destroy_connection)
			vsocket->notify_ops->destroy_connection(conn->vid);

		goto err_cleanup;
	}

	pthread_mutex_lock(&vsocket->conn_mutex);
	TAILQ_INSERT_TAIL(&vsocket->conn_list, conn, next);
	pthread_mutex_unlock(&vsocket->conn_mutex);

	fdset_pipe_notify(&vhost_user.fdset);
	return;

err_cleanup:
	vhost_destroy_device(vid);
err:
	free(conn);
	close(fd);
}

/* call back when there is new vhost-user connection from client  */
static void
vhost_user_server_new_connection(int fd, void *dat, int *remove __rte_unused)
{
	struct vhost_user_socket *vsocket = dat;

	fd = accept(fd, NULL, NULL);
	if (fd < 0)
		return;

	RTE_LOG(INFO, VHOST_CONFIG, "new vhost user connection is %d\n", fd);
	vhost_user_add_connection(fd, vsocket);
}

static void
vhost_user_read_cb(int connfd, void *dat, int *remove)
{
	struct vhost_user_connection *conn = dat;
	struct vhost_user_socket *vsocket = conn->vsocket;
	int ret;

	ret = vhost_user_msg_handler(conn->vid, connfd);
	if (ret < 0) {
		struct virtio_net *dev = get_device(conn->vid);

		close(connfd);
		*remove = 1;

		if (dev)
			vhost_destroy_device_notify(dev);

		if (vsocket->notify_ops->destroy_connection)
			vsocket->notify_ops->destroy_connection(conn->vid);

		vhost_destroy_device(conn->vid);

		pthread_mutex_lock(&vsocket->conn_mutex);
		TAILQ_REMOVE(&vsocket->conn_list, conn, next);
		pthread_mutex_unlock(&vsocket->conn_mutex);

		free(conn);

		if (vsocket->reconnect) {
			create_unix_socket(vsocket);
			vhost_user_start_client(vsocket);
		}
	}
}

static int
create_unix_socket(struct vhost_user_socket *vsocket)
{
	int fd;
	struct sockaddr_un *un = &vsocket->un;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;
	RTE_LOG(INFO, VHOST_CONFIG, "vhost-user %s: socket created, fd: %d\n",
		vsocket->is_server ? "server" : "client", fd);

	if (!vsocket->is_server && fcntl(fd, F_SETFL, O_NONBLOCK)) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"vhost-user: can't set nonblocking mode for socket, fd: "
			"%d (%s)\n", fd, strerror(errno));
		close(fd);
		return -1;
	}

	memset(un, 0, sizeof(*un));
	un->sun_family = AF_UNIX;
	strncpy(un->sun_path, vsocket->path, sizeof(un->sun_path));
	un->sun_path[sizeof(un->sun_path) - 1] = '\0';

	vsocket->socket_fd = fd;
	return 0;
}

static int
vhost_user_start_server(struct vhost_user_socket *vsocket)
{
	int ret;
	int fd = vsocket->socket_fd;
	const char *path = vsocket->path;

	/*
	 * bind () may fail if the socket file with the same name already
	 * exists. But the library obviously should not delete the file
	 * provided by the user, since we can not be sure that it is not
	 * being used by other applications. Moreover, many applications form
	 * socket names based on user input, which is prone to errors.
	 *
	 * The user must ensure that the socket does not exist before
	 * registering the vhost driver in server mode.
	 */
	ret = bind(fd, (struct sockaddr *)&vsocket->un, sizeof(vsocket->un));
	if (ret < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"failed to bind to %s: %s; remove it and try again\n",
			path, strerror(errno));
		goto err;
	}
	RTE_LOG(INFO, VHOST_CONFIG, "bind to %s\n", path);

	ret = listen(fd, MAX_VIRTIO_BACKLOG);
	if (ret < 0)
		goto err;

	ret = fdset_add(&vhost_user.fdset, fd, vhost_user_server_new_connection,
		  NULL, vsocket);
	if (ret < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"failed to add listen fd %d to vhost server fdset\n",
			fd);
		goto err;
	}

	return 0;

err:
	close(fd);
	return -1;
}

struct vhost_user_reconnect {
	struct sockaddr_un un;
	int fd;
	struct vhost_user_socket *vsocket;

	TAILQ_ENTRY(vhost_user_reconnect) next;
};

TAILQ_HEAD(vhost_user_reconnect_tailq_list, vhost_user_reconnect);
struct vhost_user_reconnect_list {
	struct vhost_user_reconnect_tailq_list head;
	pthread_mutex_t mutex;
};

static struct vhost_user_reconnect_list reconn_list;
static pthread_t reconn_tid;

static int
vhost_user_connect_nonblock(int fd, struct sockaddr *un, size_t sz)
{
	int ret, flags;

	ret = connect(fd, un, sz);
	if (ret < 0 && errno != EISCONN)
		return -1;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"can't get flags for connfd %d\n", fd);
		return -2;
	}
	if ((flags & O_NONBLOCK) && fcntl(fd, F_SETFL, flags & ~O_NONBLOCK)) {
		RTE_LOG(ERR, VHOST_CONFIG,
				"can't disable nonblocking on fd %d\n", fd);
		return -2;
	}
	return 0;
}

static void *
vhost_user_client_reconnect(void *arg __rte_unused)
{
	int ret;
	struct vhost_user_reconnect *reconn, *next;

	while (1) {
		pthread_mutex_lock(&reconn_list.mutex);

		/*
		 * An equal implementation of TAILQ_FOREACH_SAFE,
		 * which does not exist on all platforms.
		 */
		for (reconn = TAILQ_FIRST(&reconn_list.head);
		     reconn != NULL; reconn = next) {
			next = TAILQ_NEXT(reconn, next);

			ret = vhost_user_connect_nonblock(reconn->fd,
						(struct sockaddr *)&reconn->un,
						sizeof(reconn->un));
			if (ret == -2) {
				close(reconn->fd);
				RTE_LOG(ERR, VHOST_CONFIG,
					"reconnection for fd %d failed\n",
					reconn->fd);
				goto remove_fd;
			}
			if (ret == -1)
				continue;

			RTE_LOG(INFO, VHOST_CONFIG,
				"%s: connected\n", reconn->vsocket->path);
			vhost_user_add_connection(reconn->fd, reconn->vsocket);
remove_fd:
			TAILQ_REMOVE(&reconn_list.head, reconn, next);
			free(reconn);
		}

		pthread_mutex_unlock(&reconn_list.mutex);
		sleep(1);
	}

	return NULL;
}

static int
vhost_user_reconnect_init(void)
{
	int ret;

	ret = pthread_mutex_init(&reconn_list.mutex, NULL);
	if (ret < 0) {
		RTE_LOG(ERR, VHOST_CONFIG, "failed to initialize mutex");
		return ret;
	}
	TAILQ_INIT(&reconn_list.head);

	ret = rte_ctrl_thread_create(&reconn_tid, "vhost_reconn", NULL,
			     vhost_user_client_reconnect, NULL);
	if (ret != 0) {
		RTE_LOG(ERR, VHOST_CONFIG, "failed to create reconnect thread");
		if (pthread_mutex_destroy(&reconn_list.mutex)) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"failed to destroy reconnect mutex");
		}
	}

	return ret;
}

static int
vhost_user_start_client(struct vhost_user_socket *vsocket)
{
	int ret;
	int fd = vsocket->socket_fd;
	const char *path = vsocket->path;
	struct vhost_user_reconnect *reconn;

	ret = vhost_user_connect_nonblock(fd, (struct sockaddr *)&vsocket->un,
					  sizeof(vsocket->un));
	if (ret == 0) {
		vhost_user_add_connection(fd, vsocket);
		return 0;
	}

	RTE_LOG(WARNING, VHOST_CONFIG,
		"failed to connect to %s: %s\n",
		path, strerror(errno));

	if (ret == -2 || !vsocket->reconnect) {
		close(fd);
		return -1;
	}

	RTE_LOG(INFO, VHOST_CONFIG, "%s: reconnecting...\n", path);
	reconn = malloc(sizeof(*reconn));
	if (reconn == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"failed to allocate memory for reconnect\n");
		close(fd);
		return -1;
	}
	reconn->un = vsocket->un;
	reconn->fd = fd;
	reconn->vsocket = vsocket;
	pthread_mutex_lock(&reconn_list.mutex);
	TAILQ_INSERT_TAIL(&reconn_list.head, reconn, next);
	pthread_mutex_unlock(&reconn_list.mutex);

	return 0;
}

static struct vhost_user_socket *
find_vhost_user_socket(const char *path)
{
	int i;

	if (path == NULL)
		return NULL;

	for (i = 0; i < vhost_user.vsocket_cnt; i++) {
		struct vhost_user_socket *vsocket = vhost_user.vsockets[i];

		if (!strcmp(vsocket->path, path))
			return vsocket;
	}

	return NULL;
}

int
rte_vhost_driver_attach_vdpa_device(const char *path, int did)
{
	struct vhost_user_socket *vsocket;

	if (rte_vdpa_get_device(did) == NULL || path == NULL)
		return -1;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	if (vsocket)
		vsocket->vdpa_dev_id = did;
	pthread_mutex_unlock(&vhost_user.mutex);

	return vsocket ? 0 : -1;
}

int
rte_vhost_driver_detach_vdpa_device(const char *path)
{
	struct vhost_user_socket *vsocket;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	if (vsocket)
		vsocket->vdpa_dev_id = -1;
	pthread_mutex_unlock(&vhost_user.mutex);

	return vsocket ? 0 : -1;
}

int
rte_vhost_driver_get_vdpa_device_id(const char *path)
{
	struct vhost_user_socket *vsocket;
	int did = -1;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	if (vsocket)
		did = vsocket->vdpa_dev_id;
	pthread_mutex_unlock(&vhost_user.mutex);

	return did;
}

int
rte_vhost_driver_disable_features(const char *path, uint64_t features)
{
	struct vhost_user_socket *vsocket;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);

	/* Note that use_builtin_virtio_net is not affected by this function
	 * since callers may want to selectively disable features of the
	 * built-in vhost net device backend.
	 */

	if (vsocket)
		vsocket->features &= ~features;
	pthread_mutex_unlock(&vhost_user.mutex);

	return vsocket ? 0 : -1;
}

int
rte_vhost_driver_enable_features(const char *path, uint64_t features)
{
	struct vhost_user_socket *vsocket;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	if (vsocket) {
		if ((vsocket->supported_features & features) != features) {
			/*
			 * trying to enable features the driver doesn't
			 * support.
			 */
			pthread_mutex_unlock(&vhost_user.mutex);
			return -1;
		}
		vsocket->features |= features;
	}
	pthread_mutex_unlock(&vhost_user.mutex);

	return vsocket ? 0 : -1;
}

int
rte_vhost_driver_set_features(const char *path, uint64_t features)
{
	struct vhost_user_socket *vsocket;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	if (vsocket) {
		vsocket->supported_features = features;
		vsocket->features = features;

		/* Anyone setting feature bits is implementing their own vhost
		 * device backend.
		 */
		vsocket->use_builtin_virtio_net = false;
	}
	pthread_mutex_unlock(&vhost_user.mutex);

	return vsocket ? 0 : -1;
}

int
rte_vhost_driver_get_features(const char *path, uint64_t *features)
{
	struct vhost_user_socket *vsocket;
	uint64_t vdpa_features;
	struct rte_vdpa_device *vdpa_dev;
	int did = -1;
	int ret = 0;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	if (!vsocket) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"socket file %s is not registered yet.\n", path);
		ret = -1;
		goto unlock_exit;
	}

	did = vsocket->vdpa_dev_id;
	vdpa_dev = rte_vdpa_get_device(did);
	if (!vdpa_dev || !vdpa_dev->ops->get_features) {
		*features = vsocket->features;
		goto unlock_exit;
	}

	if (vdpa_dev->ops->get_features(did, &vdpa_features) < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
				"failed to get vdpa features "
				"for socket file %s.\n", path);
		ret = -1;
		goto unlock_exit;
	}

	*features = vsocket->features & vdpa_features;

unlock_exit:
	pthread_mutex_unlock(&vhost_user.mutex);
	return ret;
}

int
rte_vhost_driver_set_protocol_features(const char *path,
		uint64_t protocol_features)
{
	struct vhost_user_socket *vsocket;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	if (vsocket)
		vsocket->protocol_features = protocol_features;
	pthread_mutex_unlock(&vhost_user.mutex);
	return vsocket ? 0 : -1;
}

int
rte_vhost_driver_get_protocol_features(const char *path,
		uint64_t *protocol_features)
{
	struct vhost_user_socket *vsocket;
	uint64_t vdpa_protocol_features;
	struct rte_vdpa_device *vdpa_dev;
	int did = -1;
	int ret = 0;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	if (!vsocket) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"socket file %s is not registered yet.\n", path);
		ret = -1;
		goto unlock_exit;
	}

	did = vsocket->vdpa_dev_id;
	vdpa_dev = rte_vdpa_get_device(did);
	if (!vdpa_dev || !vdpa_dev->ops->get_protocol_features) {
		*protocol_features = vsocket->protocol_features;
		goto unlock_exit;
	}

	if (vdpa_dev->ops->get_protocol_features(did,
				&vdpa_protocol_features) < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
				"failed to get vdpa protocol features "
				"for socket file %s.\n", path);
		ret = -1;
		goto unlock_exit;
	}

	*protocol_features = vsocket->protocol_features
		& vdpa_protocol_features;

unlock_exit:
	pthread_mutex_unlock(&vhost_user.mutex);
	return ret;
}

int
rte_vhost_driver_get_queue_num(const char *path, uint32_t *queue_num)
{
	struct vhost_user_socket *vsocket;
	uint32_t vdpa_queue_num;
	struct rte_vdpa_device *vdpa_dev;
	int did = -1;
	int ret = 0;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	if (!vsocket) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"socket file %s is not registered yet.\n", path);
		ret = -1;
		goto unlock_exit;
	}

	did = vsocket->vdpa_dev_id;
	vdpa_dev = rte_vdpa_get_device(did);
	if (!vdpa_dev || !vdpa_dev->ops->get_queue_num) {
		*queue_num = VHOST_MAX_QUEUE_PAIRS;
		goto unlock_exit;
	}

	if (vdpa_dev->ops->get_queue_num(did, &vdpa_queue_num) < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
				"failed to get vdpa queue number "
				"for socket file %s.\n", path);
		ret = -1;
		goto unlock_exit;
	}

	*queue_num = RTE_MIN((uint32_t)VHOST_MAX_QUEUE_PAIRS, vdpa_queue_num);

unlock_exit:
	pthread_mutex_unlock(&vhost_user.mutex);
	return ret;
}

static void
vhost_user_socket_mem_free(struct vhost_user_socket *vsocket)
{
	if (vsocket && vsocket->path) {
		free(vsocket->path);
		vsocket->path = NULL;
	}

	if (vsocket) {
		free(vsocket);
		vsocket = NULL;
	}
}

/*
 * Register a new vhost-user socket; here we could act as server
 * (the default case), or client (when RTE_VHOST_USER_CLIENT) flag
 * is set.
 */
int
rte_vhost_driver_register(const char *path, uint64_t flags)
{
	int ret = -1;
	struct vhost_user_socket *vsocket;

	if (!path)
		return -1;

	pthread_mutex_lock(&vhost_user.mutex);

	if (vhost_user.vsocket_cnt == MAX_VHOST_SOCKET) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"error: the number of vhost sockets reaches maximum\n");
		goto out;
	}

	vsocket = malloc(sizeof(struct vhost_user_socket));
	if (!vsocket)
		goto out;
	memset(vsocket, 0, sizeof(struct vhost_user_socket));
	vsocket->path = strdup(path);
	if (vsocket->path == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"error: failed to copy socket path string\n");
		vhost_user_socket_mem_free(vsocket);
		goto out;
	}
	TAILQ_INIT(&vsocket->conn_list);
	ret = pthread_mutex_init(&vsocket->conn_mutex, NULL);
	if (ret) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"error: failed to init connection mutex\n");
		goto out_free;
	}
	vsocket->dequeue_zero_copy = flags & RTE_VHOST_USER_DEQUEUE_ZERO_COPY;

	/*
	 * Set the supported features correctly for the builtin vhost-user
	 * net driver.
	 *
	 * Applications know nothing about features the builtin virtio net
	 * driver (virtio_net.c) supports, thus it's not possible for them
	 * to invoke rte_vhost_driver_set_features(). To workaround it, here
	 * we set it unconditionally. If the application want to implement
	 * another vhost-user driver (say SCSI), it should call the
	 * rte_vhost_driver_set_features(), which will overwrite following
	 * two values.
	 */
	vsocket->use_builtin_virtio_net = true;
	vsocket->supported_features = VIRTIO_NET_SUPPORTED_FEATURES;
	vsocket->features           = VIRTIO_NET_SUPPORTED_FEATURES;
	vsocket->protocol_features  = VHOST_USER_PROTOCOL_FEATURES;

	/*
	 * Dequeue zero copy can't assure descriptors returned in order.
	 * Also, it requires that the guest memory is populated, which is
	 * not compatible with postcopy.
	 */
	if (vsocket->dequeue_zero_copy) {
		vsocket->supported_features &= ~(1ULL << VIRTIO_F_IN_ORDER);
		vsocket->features &= ~(1ULL << VIRTIO_F_IN_ORDER);

		RTE_LOG(INFO, VHOST_CONFIG,
			"Dequeue zero copy requested, disabling postcopy support\n");
		vsocket->protocol_features &=
			~(1ULL << VHOST_USER_PROTOCOL_F_PAGEFAULT);
	}

	if (!(flags & RTE_VHOST_USER_IOMMU_SUPPORT)) {
		vsocket->supported_features &= ~(1ULL << VIRTIO_F_IOMMU_PLATFORM);
		vsocket->features &= ~(1ULL << VIRTIO_F_IOMMU_PLATFORM);
	}

	if (!(flags & RTE_VHOST_USER_POSTCOPY_SUPPORT)) {
		vsocket->protocol_features &=
			~(1ULL << VHOST_USER_PROTOCOL_F_PAGEFAULT);
	} else {
#ifndef RTE_LIBRTE_VHOST_POSTCOPY
		RTE_LOG(ERR, VHOST_CONFIG,
			"Postcopy requested but not compiled\n");
		ret = -1;
		goto out_mutex;
#endif
	}

	if ((flags & RTE_VHOST_USER_CLIENT) != 0) {
		vsocket->reconnect = !(flags & RTE_VHOST_USER_NO_RECONNECT);
		if (vsocket->reconnect && reconn_tid == 0) {
			if (vhost_user_reconnect_init() != 0)
				goto out_mutex;
		}
	} else {
		vsocket->is_server = true;
	}
	ret = create_unix_socket(vsocket);
	if (ret < 0) {
		goto out_mutex;
	}

	vhost_user.vsockets[vhost_user.vsocket_cnt++] = vsocket;

	pthread_mutex_unlock(&vhost_user.mutex);
	return ret;

out_mutex:
	if (pthread_mutex_destroy(&vsocket->conn_mutex)) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"error: failed to destroy connection mutex\n");
	}
out_free:
	vhost_user_socket_mem_free(vsocket);
out:
	pthread_mutex_unlock(&vhost_user.mutex);

	return ret;
}

static bool
vhost_user_remove_reconnect(struct vhost_user_socket *vsocket)
{
	int found = false;
	struct vhost_user_reconnect *reconn, *next;

	pthread_mutex_lock(&reconn_list.mutex);

	for (reconn = TAILQ_FIRST(&reconn_list.head);
	     reconn != NULL; reconn = next) {
		next = TAILQ_NEXT(reconn, next);

		if (reconn->vsocket == vsocket) {
			TAILQ_REMOVE(&reconn_list.head, reconn, next);
			close(reconn->fd);
			free(reconn);
			found = true;
			break;
		}
	}
	pthread_mutex_unlock(&reconn_list.mutex);
	return found;
}

/**
 * Unregister the specified vhost socket
 */
int
rte_vhost_driver_unregister(const char *path)
{
	int i;
	int count;
	struct vhost_user_connection *conn, *next;

	if (path == NULL)
		return -1;

again:
	pthread_mutex_lock(&vhost_user.mutex);

	for (i = 0; i < vhost_user.vsocket_cnt; i++) {
		struct vhost_user_socket *vsocket = vhost_user.vsockets[i];

		if (!strcmp(vsocket->path, path)) {
			pthread_mutex_lock(&vsocket->conn_mutex);
			for (conn = TAILQ_FIRST(&vsocket->conn_list);
			     conn != NULL;
			     conn = next) {
				next = TAILQ_NEXT(conn, next);

				/*
				 * If r/wcb is executing, release the
				 * conn_mutex lock, and try again since
				 * the r/wcb may use the conn_mutex lock.
				 */
				if (fdset_try_del(&vhost_user.fdset,
						  conn->connfd) == -1) {
					pthread_mutex_unlock(
							&vsocket->conn_mutex);
					pthread_mutex_unlock(&vhost_user.mutex);
					goto again;
				}

				RTE_LOG(INFO, VHOST_CONFIG,
					"free connfd = %d for device '%s'\n",
					conn->connfd, path);
				close(conn->connfd);
				vhost_destroy_device(conn->vid);
				TAILQ_REMOVE(&vsocket->conn_list, conn, next);
				free(conn);
			}
			pthread_mutex_unlock(&vsocket->conn_mutex);

			if (vsocket->is_server) {
				fdset_del(&vhost_user.fdset,
						vsocket->socket_fd);
				close(vsocket->socket_fd);
				unlink(path);
			} else if (vsocket->reconnect) {
				vhost_user_remove_reconnect(vsocket);
			}

			pthread_mutex_destroy(&vsocket->conn_mutex);
			vhost_user_socket_mem_free(vsocket);

			count = --vhost_user.vsocket_cnt;
			vhost_user.vsockets[i] = vhost_user.vsockets[count];
			vhost_user.vsockets[count] = NULL;
			pthread_mutex_unlock(&vhost_user.mutex);

			return 0;
		}
	}
	pthread_mutex_unlock(&vhost_user.mutex);

	return -1;
}

/*
 * Register ops so that we can add/remove device to data core.
 */
int
rte_vhost_driver_callback_register(const char *path,
	struct vhost_device_ops const * const ops)
{
	struct vhost_user_socket *vsocket;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	if (vsocket)
		vsocket->notify_ops = ops;
	pthread_mutex_unlock(&vhost_user.mutex);

	return vsocket ? 0 : -1;
}

struct vhost_device_ops const *
vhost_driver_callback_get(const char *path)
{
	struct vhost_user_socket *vsocket;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	pthread_mutex_unlock(&vhost_user.mutex);

	return vsocket ? vsocket->notify_ops : NULL;
}

int
rte_vhost_driver_start(const char *path)
{
	struct vhost_user_socket *vsocket;
	static pthread_t fdset_tid;

	pthread_mutex_lock(&vhost_user.mutex);
	vsocket = find_vhost_user_socket(path);
	pthread_mutex_unlock(&vhost_user.mutex);

	if (!vsocket)
		return -1;

	if (fdset_tid == 0) {
		/**
		 * create a pipe which will be waited by poll and notified to
		 * rebuild the wait list of poll.
		 */
		if (fdset_pipe_init(&vhost_user.fdset) < 0) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"failed to create pipe for vhost fdset\n");
			return -1;
		}

		int ret = rte_ctrl_thread_create(&fdset_tid,
			"vhost-events", NULL, fdset_event_dispatch,
			&vhost_user.fdset);
		if (ret != 0) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"failed to create fdset handling thread");

			fdset_pipe_uninit(&vhost_user.fdset);
			return -1;
		}
	}

	if (vsocket->is_server)
		return vhost_user_start_server(vsocket);
	else
		return vhost_user_start_client(vsocket);
}
