/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2018 Intel Corporation
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <inttypes.h>
#include <libgen.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_log.h>

#include "eal_private.h"
#include "eal_filesystem.h"
#include "eal_internal_cfg.h"

static int mp_fd = -1;
static char mp_filter[PATH_MAX];   /* Filter for secondary process sockets */
static char mp_dir_path[PATH_MAX]; /* The directory path for all mp sockets */
static pthread_mutex_t mp_mutex_action = PTHREAD_MUTEX_INITIALIZER;

struct action_entry {
	TAILQ_ENTRY(action_entry) next;
	char action_name[RTE_MP_MAX_NAME_LEN];
	rte_mp_t action;
};

/** Double linked list of actions. */
TAILQ_HEAD(action_entry_list, action_entry);

static struct action_entry_list action_entry_list =
	TAILQ_HEAD_INITIALIZER(action_entry_list);

enum mp_type {
	MP_MSG, /* Share message with peers, will not block */
	MP_REQ, /* Request for information, Will block for a reply */
	MP_REP, /* Response to previously-received request */
};

struct mp_msg_internal {
	int type;
	struct rte_mp_msg msg;
};

struct sync_request {
	TAILQ_ENTRY(sync_request) next;
	int reply_received;
	char dst[PATH_MAX];
	struct rte_mp_msg *request;
	struct rte_mp_msg *reply;
	pthread_cond_t cond;
};

TAILQ_HEAD(sync_request_list, sync_request);

static struct {
	struct sync_request_list requests;
	pthread_mutex_t lock;
} sync_requests = {
	.requests = TAILQ_HEAD_INITIALIZER(sync_requests.requests),
	.lock = PTHREAD_MUTEX_INITIALIZER
};

static struct sync_request *
find_sync_request(const char *dst, const char *act_name)
{
	struct sync_request *r;

	TAILQ_FOREACH(r, &sync_requests.requests, next) {
		if (!strcmp(r->dst, dst) &&
		    !strcmp(r->request->name, act_name))
			break;
	}

	return r;
}

int
rte_eal_primary_proc_alive(const char *config_file_path)
{
	int config_fd;

	if (config_file_path)
		config_fd = open(config_file_path, O_RDONLY);
	else {
		const char *path;

		path = eal_runtime_config_path();
		config_fd = open(path, O_RDONLY);
	}
	if (config_fd < 0)
		return 0;

	int ret = lockf(config_fd, F_TEST, 0);
	close(config_fd);

	return !!ret;
}

static struct action_entry *
find_action_entry_by_name(const char *name)
{
	struct action_entry *entry;

	TAILQ_FOREACH(entry, &action_entry_list, next) {
		if (strncmp(entry->action_name, name, RTE_MP_MAX_NAME_LEN) == 0)
			break;
	}

	return entry;
}

static int
validate_action_name(const char *name)
{
	if (name == NULL) {
		RTE_LOG(ERR, EAL, "Action name cannot be NULL\n");
		rte_errno = EINVAL;
		return -1;
	}
	if (strnlen(name, RTE_MP_MAX_NAME_LEN) == 0) {
		RTE_LOG(ERR, EAL, "Length of action name is zero\n");
		rte_errno = EINVAL;
		return -1;
	}
	if (strnlen(name, RTE_MP_MAX_NAME_LEN) == RTE_MP_MAX_NAME_LEN) {
		rte_errno = E2BIG;
		return -1;
	}
	return 0;
}

int __rte_experimental
rte_mp_action_register(const char *name, rte_mp_t action)
{
	struct action_entry *entry;

	if (validate_action_name(name))
		return -1;

	entry = malloc(sizeof(struct action_entry));
	if (entry == NULL) {
		rte_errno = ENOMEM;
		return -1;
	}
	strcpy(entry->action_name, name);
	entry->action = action;

	pthread_mutex_lock(&mp_mutex_action);
	if (find_action_entry_by_name(name) != NULL) {
		pthread_mutex_unlock(&mp_mutex_action);
		rte_errno = EEXIST;
		free(entry);
		return -1;
	}
	TAILQ_INSERT_TAIL(&action_entry_list, entry, next);
	pthread_mutex_unlock(&mp_mutex_action);
	return 0;
}

void __rte_experimental
rte_mp_action_unregister(const char *name)
{
	struct action_entry *entry;

	if (validate_action_name(name))
		return;

	pthread_mutex_lock(&mp_mutex_action);
	entry = find_action_entry_by_name(name);
	if (entry == NULL) {
		pthread_mutex_unlock(&mp_mutex_action);
		return;
	}
	TAILQ_REMOVE(&action_entry_list, entry, next);
	pthread_mutex_unlock(&mp_mutex_action);
	free(entry);
}

static int
read_msg(struct mp_msg_internal *m, struct sockaddr_un *s)
{
	int msglen;
	struct iovec iov;
	struct msghdr msgh;
	char control[CMSG_SPACE(sizeof(m->msg.fds))];
	struct cmsghdr *cmsg;
	int buflen = sizeof(*m) - sizeof(m->msg.fds);

	memset(&msgh, 0, sizeof(msgh));
	iov.iov_base = m;
	iov.iov_len  = buflen;

	msgh.msg_name = s;
	msgh.msg_namelen = sizeof(*s);
	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;
	msgh.msg_control = control;
	msgh.msg_controllen = sizeof(control);

	msglen = recvmsg(mp_fd, &msgh, 0);
	if (msglen < 0) {
		RTE_LOG(ERR, EAL, "recvmsg failed, %s\n", strerror(errno));
		return -1;
	}

	if (msglen != buflen || (msgh.msg_flags & (MSG_TRUNC | MSG_CTRUNC))) {
		RTE_LOG(ERR, EAL, "truncted msg\n");
		return -1;
	}

	/* read auxiliary FDs if any */
	for (cmsg = CMSG_FIRSTHDR(&msgh); cmsg != NULL;
		cmsg = CMSG_NXTHDR(&msgh, cmsg)) {
		if ((cmsg->cmsg_level == SOL_SOCKET) &&
			(cmsg->cmsg_type == SCM_RIGHTS)) {
			memcpy(m->msg.fds, CMSG_DATA(cmsg), sizeof(m->msg.fds));
			break;
		}
	}

	return 0;
}

static void
process_msg(struct mp_msg_internal *m, struct sockaddr_un *s)
{
	struct sync_request *sync_req;
	struct action_entry *entry;
	struct rte_mp_msg *msg = &m->msg;
	rte_mp_t action = NULL;

	RTE_LOG(DEBUG, EAL, "msg: %s\n", msg->name);

	if (m->type == MP_REP) {
		pthread_mutex_lock(&sync_requests.lock);
		sync_req = find_sync_request(s->sun_path, msg->name);
		if (sync_req) {
			memcpy(sync_req->reply, msg, sizeof(*msg));
			sync_req->reply_received = 1;
			pthread_cond_signal(&sync_req->cond);
		} else
			RTE_LOG(ERR, EAL, "Drop mp reply: %s\n", msg->name);
		pthread_mutex_unlock(&sync_requests.lock);
		return;
	}

	pthread_mutex_lock(&mp_mutex_action);
	entry = find_action_entry_by_name(msg->name);
	if (entry != NULL)
		action = entry->action;
	pthread_mutex_unlock(&mp_mutex_action);

	if (!action)
		RTE_LOG(ERR, EAL, "Cannot find action: %s\n", msg->name);
	else if (action(msg, s->sun_path) < 0)
		RTE_LOG(ERR, EAL, "Fail to handle message: %s\n", msg->name);
}

static void *
mp_handle(void *arg __rte_unused)
{
	struct mp_msg_internal msg;
	struct sockaddr_un sa;

	while (1) {
		if (read_msg(&msg, &sa) == 0)
			process_msg(&msg, &sa);
	}

	return NULL;
}

static int
open_socket_fd(void)
{
	struct sockaddr_un un;
	const char *prefix = eal_mp_socket_path();

	mp_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (mp_fd < 0) {
		RTE_LOG(ERR, EAL, "failed to create unix socket\n");
		return -1;
	}

	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		snprintf(un.sun_path, sizeof(un.sun_path), "%s", prefix);
	else {
		snprintf(un.sun_path, sizeof(un.sun_path), "%s_%d_%"PRIx64,
			 prefix, getpid(), rte_rdtsc());
	}
	unlink(un.sun_path); /* May still exist since last run */
	if (bind(mp_fd, (struct sockaddr *)&un, sizeof(un)) < 0) {
		RTE_LOG(ERR, EAL, "failed to bind %s: %s\n",
			un.sun_path, strerror(errno));
		close(mp_fd);
		return -1;
	}

	RTE_LOG(INFO, EAL, "Multi-process socket %s\n", un.sun_path);
	return mp_fd;
}

static int
unlink_sockets(const char *filter)
{
	int dir_fd;
	DIR *mp_dir;
	struct dirent *ent;

	mp_dir = opendir(mp_dir_path);
	if (!mp_dir) {
		RTE_LOG(ERR, EAL, "Unable to open directory %s\n", mp_dir_path);
		return -1;
	}
	dir_fd = dirfd(mp_dir);

	while ((ent = readdir(mp_dir))) {
		if (fnmatch(filter, ent->d_name, 0) == 0)
			unlinkat(dir_fd, ent->d_name, 0);
	}

	closedir(mp_dir);
	return 0;
}

static void
unlink_socket_by_path(const char *path)
{
	char *filename;
	char *fullpath = strdup(path);

	if (!fullpath)
		return;
	filename = basename(fullpath);
	unlink_sockets(filename);
	free(fullpath);
	RTE_LOG(INFO, EAL, "Remove socket %s\n", path);
}

int
rte_mp_channel_init(void)
{
	char thread_name[RTE_MAX_THREAD_NAME_LEN];
	char *path;
	pthread_t tid;

	snprintf(mp_filter, PATH_MAX, ".%s_unix_*",
		 internal_config.hugefile_prefix);

	path = strdup(eal_mp_socket_path());
	snprintf(mp_dir_path, PATH_MAX, "%s", dirname(path));
	free(path);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY &&
	    unlink_sockets(mp_filter)) {
		RTE_LOG(ERR, EAL, "failed to unlink mp sockets\n");
		return -1;
	}

	if (open_socket_fd() < 0)
		return -1;

	if (pthread_create(&tid, NULL, mp_handle, NULL) < 0) {
		RTE_LOG(ERR, EAL, "failed to create mp thead: %s\n",
			strerror(errno));
		close(mp_fd);
		mp_fd = -1;
		return -1;
	}

	/* try best to set thread name */
	snprintf(thread_name, RTE_MAX_THREAD_NAME_LEN, "rte_mp_handle");
	rte_thread_setname(tid, thread_name);
	return 0;
}

/**
 * Return -1, as fail to send message and it's caused by the local side.
 * Return 0, as fail to send message and it's caused by the remote side.
 * Return 1, as succeed to send message.
 *
 */
static int
send_msg(const char *dst_path, struct rte_mp_msg *msg, int type)
{
	int snd;
	struct iovec iov;
	struct msghdr msgh;
	struct cmsghdr *cmsg;
	struct sockaddr_un dst;
	struct mp_msg_internal m;
	int fd_size = msg->num_fds * sizeof(int);
	char control[CMSG_SPACE(fd_size)];

	m.type = type;
	memcpy(&m.msg, msg, sizeof(*msg));

	memset(&dst, 0, sizeof(dst));
	dst.sun_family = AF_UNIX;
	snprintf(dst.sun_path, sizeof(dst.sun_path), "%s", dst_path);

	memset(&msgh, 0, sizeof(msgh));
	memset(control, 0, sizeof(control));

	iov.iov_base = &m;
	iov.iov_len = sizeof(m) - sizeof(msg->fds);

	msgh.msg_name = &dst;
	msgh.msg_namelen = sizeof(dst);
	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;
	msgh.msg_control = control;
	msgh.msg_controllen = sizeof(control);

	cmsg = CMSG_FIRSTHDR(&msgh);
	cmsg->cmsg_len = CMSG_LEN(fd_size);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	memcpy(CMSG_DATA(cmsg), msg->fds, fd_size);

	do {
		snd = sendmsg(mp_fd, &msgh, 0);
	} while (snd < 0 && errno == EINTR);

	if (snd < 0) {
		rte_errno = errno;
		/* Check if it caused by peer process exits */
		if (errno == -ECONNREFUSED) {
			/* We don't unlink the primary's socket here */
			if (rte_eal_process_type() == RTE_PROC_PRIMARY)
				unlink_socket_by_path(dst_path);
			return 0;
		}
		if (errno == -ENOBUFS) {
			RTE_LOG(ERR, EAL, "Peer cannot receive message %s\n",
				dst_path);
			return 0;
		}
		RTE_LOG(ERR, EAL, "failed to send to (%s) due to %s\n",
			dst_path, strerror(errno));
		return -1;
	}

	return 1;
}

static int
mp_send(struct rte_mp_msg *msg, const char *peer, int type)
{
	int ret = 0;
	DIR *mp_dir;
	struct dirent *ent;

	if (!peer && (rte_eal_process_type() == RTE_PROC_SECONDARY))
		peer = eal_mp_socket_path();

	if (peer) {
		if (send_msg(peer, msg, type) < 0)
			return -1;
		else
			return 0;
	}

	/* broadcast to all secondary processes */
	mp_dir = opendir(mp_dir_path);
	if (!mp_dir) {
		RTE_LOG(ERR, EAL, "Unable to open directory %s\n",
				mp_dir_path);
		rte_errno = errno;
		return -1;
	}
	while ((ent = readdir(mp_dir))) {
		if (fnmatch(mp_filter, ent->d_name, 0) != 0)
			continue;

		if (send_msg(ent->d_name, msg, type) < 0)
			ret = -1;
	}

	closedir(mp_dir);
	return ret;
}

static bool
check_input(const struct rte_mp_msg *msg)
{
	if (msg == NULL) {
		RTE_LOG(ERR, EAL, "Msg cannot be NULL\n");
		rte_errno = EINVAL;
		return false;
	}

	if (validate_action_name(msg->name))
		return false;

	if (msg->len_param > RTE_MP_MAX_PARAM_LEN) {
		RTE_LOG(ERR, EAL, "Message data is too long\n");
		rte_errno = E2BIG;
		return false;
	}

	if (msg->num_fds > RTE_MP_MAX_FD_NUM) {
		RTE_LOG(ERR, EAL, "Cannot send more than %d FDs\n",
			RTE_MP_MAX_FD_NUM);
		rte_errno = E2BIG;
		return false;
	}

	return true;
}

int __rte_experimental
rte_mp_sendmsg(struct rte_mp_msg *msg)
{
	if (!check_input(msg))
		return -1;

	RTE_LOG(DEBUG, EAL, "sendmsg: %s\n", msg->name);
	return mp_send(msg, NULL, MP_MSG);
}

static int
mp_request_one(const char *dst, struct rte_mp_msg *req,
	       struct rte_mp_reply *reply, const struct timespec *ts)
{
	int ret;
	struct timeval now;
	struct rte_mp_msg msg, *tmp;
	struct sync_request sync_req, *exist;

	sync_req.reply_received = 0;
	strcpy(sync_req.dst, dst);
	sync_req.request = req;
	sync_req.reply = &msg;
	pthread_cond_init(&sync_req.cond, NULL);

	pthread_mutex_lock(&sync_requests.lock);
	exist = find_sync_request(dst, req->name);
	if (!exist)
		TAILQ_INSERT_TAIL(&sync_requests.requests, &sync_req, next);
	pthread_mutex_unlock(&sync_requests.lock);
	if (exist) {
		RTE_LOG(ERR, EAL, "A pending request %s:%s\n", dst, req->name);
		rte_errno = EEXIST;
		return -1;
	}

	ret = send_msg(dst, req, MP_REQ);
	if (ret < 0) {
		RTE_LOG(ERR, EAL, "Fail to send request %s:%s\n",
			dst, req->name);
		return -1;
	} else if (ret == 0)
		return 0;

	reply->nb_sent++;

	pthread_mutex_lock(&sync_requests.lock);
	do {
		pthread_cond_timedwait(&sync_req.cond, &sync_requests.lock, ts);
		/* Check spurious wakeups */
		if (sync_req.reply_received == 1)
			break;
		/* Check if time is out */
		if (gettimeofday(&now, NULL) < 0)
			break;
		if (now.tv_sec < ts->tv_sec)
			break;
		else if (now.tv_sec == ts->tv_sec &&
			 now.tv_usec * 1000 < ts->tv_nsec)
			break;
	} while (1);
	/* We got the lock now */
	TAILQ_REMOVE(&sync_requests.requests, &sync_req, next);
	pthread_mutex_unlock(&sync_requests.lock);

	if (sync_req.reply_received == 0) {
		RTE_LOG(ERR, EAL, "Fail to recv reply for request %s:%s\n",
			dst, req->name);
		rte_errno = ETIMEDOUT;
		return -1;
	}

	tmp = realloc(reply->msgs, sizeof(msg) * (reply->nb_received + 1));
	if (!tmp) {
		RTE_LOG(ERR, EAL, "Fail to alloc reply for request %s:%s\n",
			dst, req->name);
		rte_errno = ENOMEM;
		return -1;
	}
	memcpy(&tmp[reply->nb_received], &msg, sizeof(msg));
	reply->msgs = tmp;
	reply->nb_received++;
	return 0;
}

int __rte_experimental
rte_mp_request(struct rte_mp_msg *req, struct rte_mp_reply *reply,
		const struct timespec *ts)
{
	int ret = 0;
	DIR *mp_dir;
	struct dirent *ent;
	struct timeval now;
	struct timespec end;

	RTE_LOG(DEBUG, EAL, "request: %s\n", req->name);

	if (check_input(req) == false)
		return -1;
	if (gettimeofday(&now, NULL) < 0) {
		RTE_LOG(ERR, EAL, "Faile to get current time\n");
		rte_errno = errno;
		return -1;
	}

	end.tv_nsec = (now.tv_usec * 1000 + ts->tv_nsec) % 1000000000;
	end.tv_sec = now.tv_sec + ts->tv_sec +
			(now.tv_usec * 1000 + ts->tv_nsec) / 1000000000;

	reply->nb_sent = 0;
	reply->nb_received = 0;
	reply->msgs = NULL;

	/* for secondary process, send request to the primary process only */
	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return mp_request_one(eal_mp_socket_path(), req, reply, &end);

	/* for primary process, broadcast request, and collect reply 1 by 1 */
	mp_dir = opendir(mp_dir_path);
	if (!mp_dir) {
		RTE_LOG(ERR, EAL, "Unable to open directory %s\n", mp_dir_path);
		rte_errno = errno;
		return -1;
	}

	while ((ent = readdir(mp_dir))) {
		if (fnmatch(mp_filter, ent->d_name, 0) != 0)
			continue;

		if (mp_request_one(ent->d_name, req, reply, &end))
			ret = -1;
	}

	closedir(mp_dir);
	return ret;
}

int __rte_experimental
rte_mp_reply(struct rte_mp_msg *msg, const char *peer)
{

	RTE_LOG(DEBUG, EAL, "reply: %s\n", msg->name);

	if (check_input(msg) == false)
		return -1;

	if (peer == NULL) {
		RTE_LOG(ERR, EAL, "peer is not specified\n");
		rte_errno = EINVAL;
		return -1;
	}

	return mp_send(msg, peer, MP_REP);
}
