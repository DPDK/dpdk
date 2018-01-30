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
		rte_errno = -EINVAL;
		return -1;
	}
	if (strnlen(name, RTE_MP_MAX_NAME_LEN) == 0) {
		RTE_LOG(ERR, EAL, "Length of action name is zero\n");
		rte_errno = -EINVAL;
		return -1;
	}
	if (strnlen(name, RTE_MP_MAX_NAME_LEN) == RTE_MP_MAX_NAME_LEN) {
		rte_errno = -E2BIG;
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
		rte_errno = -ENOMEM;
		return -1;
	}
	strcpy(entry->action_name, name);
	entry->action = action;

	pthread_mutex_lock(&mp_mutex_action);
	if (find_action_entry_by_name(name) != NULL) {
		pthread_mutex_unlock(&mp_mutex_action);
		rte_errno = -EEXIST;
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
read_msg(struct rte_mp_msg *msg)
{
	int msglen;
	struct iovec iov;
	struct msghdr msgh;
	char control[CMSG_SPACE(sizeof(msg->fds))];
	struct cmsghdr *cmsg;
	int buflen = sizeof(*msg) - sizeof(msg->fds);

	memset(&msgh, 0, sizeof(msgh));
	iov.iov_base = msg;
	iov.iov_len  = buflen;

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
			memcpy(msg->fds, CMSG_DATA(cmsg), sizeof(msg->fds));
			break;
		}
	}

	return 0;
}

static void
process_msg(struct rte_mp_msg *msg)
{
	struct action_entry *entry;
	rte_mp_t action = NULL;

	RTE_LOG(DEBUG, EAL, "msg: %s\n", msg->name);
	pthread_mutex_lock(&mp_mutex_action);
	entry = find_action_entry_by_name(msg->name);
	if (entry != NULL)
		action = entry->action;
	pthread_mutex_unlock(&mp_mutex_action);

	if (!action)
		RTE_LOG(ERR, EAL, "Cannot find action: %s\n", msg->name);
	else if (action(msg) < 0)
		RTE_LOG(ERR, EAL, "Fail to handle message: %s\n", msg->name);
}

static void *
mp_handle(void *arg __rte_unused)
{
	struct rte_mp_msg msg;

	while (1) {
		if (read_msg(&msg) == 0)
			process_msg(&msg);
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
send_msg(const char *dst_path, struct rte_mp_msg *msg)
{
	int snd;
	struct iovec iov;
	struct msghdr msgh;
	struct cmsghdr *cmsg;
	struct sockaddr_un dst;
	int fd_size = msg->num_fds * sizeof(int);
	char control[CMSG_SPACE(fd_size)];

	memset(&dst, 0, sizeof(dst));
	dst.sun_family = AF_UNIX;
	snprintf(dst.sun_path, sizeof(dst.sun_path), "%s", dst_path);

	memset(&msgh, 0, sizeof(msgh));
	memset(control, 0, sizeof(control));

	iov.iov_base = msg;
	iov.iov_len = sizeof(*msg) - sizeof(msg->fds);

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
mp_send(struct rte_mp_msg *msg)
{
	int ret = 0;
	DIR *mp_dir;
	struct dirent *ent;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		if (send_msg(eal_mp_socket_path(), msg) < 0)
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

		if (send_msg(ent->d_name, msg) < 0)
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
		rte_errno = -EINVAL;
		return false;
	}

	if (validate_action_name(msg->name))
		return false;

	if (msg->len_param > RTE_MP_MAX_PARAM_LEN) {
		RTE_LOG(ERR, EAL, "Message data is too long\n");
		rte_errno = -E2BIG;
		return false;
	}

	if (msg->num_fds > RTE_MP_MAX_FD_NUM) {
		RTE_LOG(ERR, EAL, "Cannot send more than %d FDs\n",
			RTE_MP_MAX_FD_NUM);
		rte_errno = -E2BIG;
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
	return mp_send(msg);
}
