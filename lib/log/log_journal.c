/* SPDX-License-Identifier: BSD-3-Clause */

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

#include <rte_log.h>

#include "log_private.h"

/*
 * Send structured message using journal protocol
 * See: https://systemd.io/JOURNAL_NATIVE_PROTOCOL/
 *
 * Uses writev() to ensure that whole log message is in one datagram
 */
static int
journal_send(int fd, const char *buf, size_t len)
{
	struct iovec iov[4];
	unsigned int n = 0;
	int priority = rte_log_cur_msg_loglevel() - 1;
	char msg[] = "MESSAGE=";
	char newline = '\n';
	char pbuf[32];	/* "PRIORITY=N\n" */

	iov[n].iov_base = msg;
	iov[n++].iov_len = strlen(msg);

	iov[n].iov_base = (char *)(uintptr_t)buf;
	iov[n++].iov_len = len;

	/* if message doesn't end with newline, one will be applied. */
	if (buf[len - 1] != '\n') {
		iov[n].iov_base = &newline;
		iov[n++].iov_len = 1;
	}

	/* priority value between 0 ("emerg") and 7 ("debug") */
	iov[n].iov_base = pbuf;
	iov[n++].iov_len = snprintf(pbuf, sizeof(pbuf),
				    "PRIORITY=%d\n", priority);
	return writev(fd, iov, n);
}


/* wrapper for log stream to put messages into journal */
static ssize_t
journal_log_write(void *c, const char *buf, size_t size)
{
	int fd = (uintptr_t)c;

	return journal_send(fd, buf, size);
}

static int
journal_log_close(void *c)
{
	int fd = (uintptr_t)c;

	close(fd);
	return 0;
}

static cookie_io_functions_t journal_log_func = {
	.write = journal_log_write,
	.close = journal_log_close,
};

/*
 * Check if stderr is going to system journal.
 * This is the documented way to handle systemd journal
 *
 * See: https://systemd.io/JOURNAL_NATIVE_PROTOCOL/
 */
bool
log_journal_enabled(void)
{
	char *jenv, *endp = NULL;
	struct stat st;
	unsigned long dev, ino;

	jenv = getenv("JOURNAL_STREAM");
	if (jenv == NULL)
		return false;

	if (fstat(STDERR_FILENO, &st) < 0)
		return false;

	/* systemd sets colon-separated list of device and inode number */
	dev = strtoul(jenv, &endp, 10);
	if (endp == NULL || *endp != ':')
		return false;	/* missing colon */

	ino = strtoul(endp + 1, NULL, 10);

	return dev == st.st_dev && ino == st.st_ino;
}

/* Connect to systemd's journal service */
FILE *
log_journal_open(const char *id)
{
	char syslog_id[PATH_MAX];
	FILE *log_stream;
	int len;
	struct sockaddr_un sun = {
		.sun_family = AF_UNIX,
		.sun_path = "/run/systemd/journal/socket",
	};
	int jfd;

	len = snprintf(syslog_id, sizeof(syslog_id),
		       "SYSLOG_IDENTIFIER=%s\nSYSLOG_PID=%u", id, getpid());

	/* Detect truncation of message and fallback to no journal */
	if (len >= (int)sizeof(syslog_id))
		return NULL;

	jfd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (jfd < 0) {
		perror("socket");
		return NULL;
	}

	if (connect(jfd, (struct sockaddr *)&sun, sizeof(sun)) < 0) {
		perror("connect");
		goto error;
	}

	/* Send identifier as first message */
	if (write(jfd, syslog_id, len) != len) {
		perror("write");
		goto error;
	}

	/* redirect other log messages to journal */
	log_stream = fopencookie((void *)(uintptr_t)jfd, "w", journal_log_func);
	if (log_stream != NULL)
		return log_stream;

error:
	close(jfd);
	return NULL;
}
