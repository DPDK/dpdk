/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <rte_string_fns.h>

#include "module_api.h"

#define MSG_CMD_TOO_LONG "Command too long."

static int
data_event_handle(struct conn *conn, int fd_client)
{
	ssize_t len, i, rc = 0;

	/* Read input message */
	len = read(fd_client, conn->buf, conn->buf_size);
	if (len == -1) {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			return 0;

		return -1;
	}

	if (len == 0)
		return rc;

	/* Handle input messages */
	for (i = 0; i < len; i++) {
		if (conn->buf[i] == '\n') {
			size_t n;

			conn->msg_in[conn->msg_in_len] = 0;
			conn->msg_out[0] = 0;

			conn->msg_handle(conn->msg_in, conn->msg_out, conn->msg_out_len_max,
					 conn->msg_handle_arg);

			n = strlen(conn->msg_out);
			if (n) {
				rc = write(fd_client, conn->msg_out, n);
				if (rc == -1)
					goto exit;
			}

			conn->msg_in_len = 0;
		} else if (conn->msg_in_len < conn->msg_in_len_max) {
			conn->msg_in[conn->msg_in_len] = conn->buf[i];
			conn->msg_in_len++;
		} else {
			rc = write(fd_client, MSG_CMD_TOO_LONG, strlen(MSG_CMD_TOO_LONG));
			if (rc == -1)
				goto exit;

			conn->msg_in_len = 0;
		}
	}

	/* Write prompt */
	rc = write(fd_client, conn->prompt, strlen(conn->prompt));
	rc = (rc == -1) ? -1 : 0;

exit:
	return rc;
}

static int
control_event_handle(struct conn *conn, int fd_client)
{
	int rc;

	rc = epoll_ctl(conn->fd_client_group, EPOLL_CTL_DEL, fd_client, NULL);
	if (rc == -1)
		goto exit;

	rc = close(fd_client);
	if (rc == -1)
		goto exit;

	rc = 0;

exit:
	return rc;
}

struct conn *
conn_init(struct conn_params *p)
{
	int fd_server, fd_client_group, rc;
	struct sockaddr_in server_address;
	struct conn *conn = NULL;
	int reuse = 1;

	memset(&server_address, 0, sizeof(server_address));

	/* Check input arguments */
	if ((p == NULL) || (p->welcome == NULL) || (p->prompt == NULL) || (p->addr == NULL) ||
	    (p->buf_size == 0) || (p->msg_in_len_max == 0) || (p->msg_out_len_max == 0) ||
	    (p->msg_handle == NULL))
		goto exit;

	rc = inet_aton(p->addr, &server_address.sin_addr);
	if (rc == 0)
		goto exit;

	/* Memory allocation */
	conn = calloc(1, sizeof(struct conn));
	if (conn == NULL)
		goto exit;

	conn->welcome = calloc(1, CONN_WELCOME_LEN_MAX + 1);
	conn->prompt = calloc(1, CONN_PROMPT_LEN_MAX + 1);
	conn->buf = calloc(1, p->buf_size);
	conn->msg_in = calloc(1, p->msg_in_len_max + 1);
	conn->msg_out = calloc(1, p->msg_out_len_max + 1);

	if ((conn->welcome == NULL) || (conn->prompt == NULL) || (conn->buf == NULL) ||
	    (conn->msg_in == NULL) || (conn->msg_out == NULL)) {
		conn_free(conn);
		conn = NULL;
		goto exit;
	}

	/* Server socket */
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(p->port);

	fd_server = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (fd_server == -1) {
		conn_free(conn);
		conn = NULL;
		goto exit;
	}

	if (setsockopt(fd_server, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse,
		       sizeof(reuse)) < 0)
		goto free;

	rc = bind(fd_server, (struct sockaddr *)&server_address, sizeof(server_address));
	if (rc == -1)
		goto free;

	rc = listen(fd_server, 16);
	if (rc == -1)
		goto free;

	/* Client group */
	fd_client_group = epoll_create(1);
	if (fd_client_group == -1)
		goto free;

	/* Fill in */
	rte_strscpy(conn->welcome, p->welcome, CONN_WELCOME_LEN_MAX);
	rte_strscpy(conn->prompt, p->prompt, CONN_PROMPT_LEN_MAX);
	conn->buf_size = p->buf_size;
	conn->msg_in_len_max = p->msg_in_len_max;
	conn->msg_out_len_max = p->msg_out_len_max;
	conn->msg_in_len = 0;
	conn->fd_server = fd_server;
	conn->fd_client_group = fd_client_group;
	conn->msg_handle = p->msg_handle;
	conn->msg_handle_arg = p->msg_handle_arg;

exit:
	return conn;
free:
	conn_free(conn);
	close(fd_server);
	conn = NULL;
	return conn;
}

void
conn_free(struct conn *conn)
{
	if (conn == NULL)
		return;

	if (conn->fd_client_group)
		close(conn->fd_client_group);

	if (conn->fd_server)
		close(conn->fd_server);

	free(conn->msg_out);
	free(conn->msg_in);
	free(conn->prompt);
	free(conn->welcome);
	free(conn);
}

int
conn_req_poll(struct conn *conn)
{
	struct sockaddr_in client_address;
	socklen_t client_address_length;
	struct epoll_event event;
	int fd_client, rc;

	/* Check input arguments */
	if (conn == NULL)
		return -1;

	/* Server socket */
	client_address_length = sizeof(client_address);
	fd_client = accept4(conn->fd_server, (struct sockaddr *)&client_address,
			    &client_address_length, SOCK_NONBLOCK);
	if (fd_client == -1) {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			return 0;

		return -1;
	}

	/* Client group */
	event.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP;
	event.data.fd = fd_client;

	rc = epoll_ctl(conn->fd_client_group, EPOLL_CTL_ADD, fd_client, &event);
	if (rc == -1) {
		close(fd_client);
		goto exit;
	}

	/* Client */
	rc = write(fd_client, conn->welcome, strlen(conn->welcome));
	if (rc == -1) {
		close(fd_client);
		goto exit;
	}

	rc = write(fd_client, conn->prompt, strlen(conn->prompt));
	if (rc == -1) {
		close(fd_client);
		goto exit;
	}

	rc = 0;

exit:
	return rc;
}

int
conn_msg_poll(struct conn *conn)
{
	int fd_client, rc, rc_data = 0, rc_control = 0;
	struct epoll_event event;

	/* Check input arguments */
	if (conn == NULL)
		return -1;

	/* Client group */
	rc = epoll_wait(conn->fd_client_group, &event, 1, 0);
	if ((rc == -1) || rc == 0)
		return rc;

	fd_client = event.data.fd;

	/* Data available */
	if (event.events & EPOLLIN)
		rc_data = data_event_handle(conn, fd_client);

	/* Control events */
	if (event.events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP))
		rc_control = control_event_handle(conn, fd_client);

	if (rc_data || rc_control)
		return -1;

	return 0;
}
