/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#include <sys/socket.h>

#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "conn.h"

#define MSG_CMD_TOO_LONG "Command too long."

struct softnic_conn {
	char *welcome;
	char *prompt;
	char *buf;
	char *msg_in;
	char *msg_out;
	size_t buf_size;
	size_t msg_in_len_max;
	size_t msg_out_len_max;
	size_t msg_in_len;
	int fd_server;
	int fd_client_group;
	softnic_conn_msg_handle_t msg_handle;
	void *msg_handle_arg;
};

struct softnic_conn *
softnic_conn_init(struct softnic_conn_params *p)
{
	struct sockaddr_in server_address;
	struct softnic_conn *conn;
	int fd_server, fd_client_group, status;

	memset(&server_address, 0, sizeof(server_address));

	/* Check input arguments */
	if (p == NULL ||
		p->welcome == NULL ||
		p->prompt == NULL ||
		p->addr == NULL ||
		p->buf_size == 0 ||
		p->msg_in_len_max == 0 ||
		p->msg_out_len_max == 0 ||
		p->msg_handle == NULL)
		return NULL;

	status = inet_aton(p->addr, &server_address.sin_addr);
	if (status == 0)
		return NULL;

	/* Memory allocation */
	conn = calloc(1, sizeof(struct softnic_conn));
	if (conn == NULL)
		return NULL;

	conn->welcome = calloc(1, CONN_WELCOME_LEN_MAX + 1);
	conn->prompt = calloc(1, CONN_PROMPT_LEN_MAX + 1);
	conn->buf = calloc(1, p->buf_size);
	conn->msg_in = calloc(1, p->msg_in_len_max + 1);
	conn->msg_out = calloc(1, p->msg_out_len_max + 1);

	if (conn->welcome == NULL ||
		conn->prompt == NULL ||
		conn->buf == NULL ||
		conn->msg_in == NULL ||
		conn->msg_out == NULL) {
		softnic_conn_free(conn);
		return NULL;
	}

	/* Server socket */
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(p->port);

	fd_server = socket(AF_INET,
		SOCK_STREAM | SOCK_NONBLOCK,
		0);
	if (fd_server == -1) {
		softnic_conn_free(conn);
		return NULL;
	}

	status = bind(fd_server,
		(struct sockaddr *)&server_address,
		sizeof(server_address));
	if (status == -1) {
		softnic_conn_free(conn);
		close(fd_server);
		return NULL;
	}

	status = listen(fd_server, 16);
	if (status == -1) {
		softnic_conn_free(conn);
		close(fd_server);
		return NULL;
	}

	/* Client group */
	fd_client_group = epoll_create(1);
	if (fd_client_group == -1) {
		softnic_conn_free(conn);
		close(fd_server);
		return NULL;
	}

	/* Fill in */
	strncpy(conn->welcome, p->welcome, CONN_WELCOME_LEN_MAX);
	strncpy(conn->prompt, p->prompt, CONN_PROMPT_LEN_MAX);
	conn->buf_size = p->buf_size;
	conn->msg_in_len_max = p->msg_in_len_max;
	conn->msg_out_len_max = p->msg_out_len_max;
	conn->msg_in_len = 0;
	conn->fd_server = fd_server;
	conn->fd_client_group = fd_client_group;
	conn->msg_handle = p->msg_handle;
	conn->msg_handle_arg = p->msg_handle_arg;

	return conn;
}

void
softnic_conn_free(struct softnic_conn *conn)
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
softnic_conn_poll_for_conn(struct softnic_conn *conn)
{
	struct sockaddr_in client_address;
	struct epoll_event event;
	socklen_t client_address_length;
	int fd_client, status;

	/* Check input arguments */
	if (conn == NULL)
		return -1;

	/* Server socket */
	client_address_length = sizeof(client_address);
	fd_client = accept4(conn->fd_server,
		(struct sockaddr *)&client_address,
		&client_address_length,
		SOCK_NONBLOCK);
	if (fd_client == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;

		return -1;
	}

	/* Client group */
	event.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP;
	event.data.fd = fd_client;

	status = epoll_ctl(conn->fd_client_group,
		EPOLL_CTL_ADD,
		fd_client,
		&event);
	if (status == -1) {
		close(fd_client);
		return -1;
	}

	/* Client */
	status = write(fd_client,
		conn->welcome,
		strlen(conn->welcome));
	if (status == -1) {
		close(fd_client);
		return -1;
	}

	status = write(fd_client,
		conn->prompt,
		strlen(conn->prompt));
	if (status == -1) {
		close(fd_client);
		return -1;
	}

	return 0;
}

static int
data_event_handle(struct softnic_conn *conn,
	int fd_client)
{
	ssize_t len, i, status;

	/* Read input message */

	len = read(fd_client,
		conn->buf,
		conn->buf_size);
	if (len == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;

		return -1;
	}
	if (len == 0)
		return 0;

	/* Handle input messages */
	for (i = 0; i < len; i++) {
		if (conn->buf[i] == '\n') {
			size_t n;

			conn->msg_in[conn->msg_in_len] = 0;
			conn->msg_out[0] = 0;

			conn->msg_handle(conn->msg_in,
				conn->msg_out,
				conn->msg_out_len_max,
				conn->msg_handle_arg);

			n = strlen(conn->msg_out);
			if (n) {
				status = write(fd_client,
					conn->msg_out,
					n);
				if (status == -1)
					return status;
			}

			conn->msg_in_len = 0;
		} else if (conn->msg_in_len < conn->msg_in_len_max) {
			conn->msg_in[conn->msg_in_len] = conn->buf[i];
			conn->msg_in_len++;
		} else {
			status = write(fd_client,
				MSG_CMD_TOO_LONG,
				strlen(MSG_CMD_TOO_LONG));
			if (status == -1)
				return status;

			conn->msg_in_len = 0;
		}
	}

	/* Write prompt */
	status = write(fd_client,
		conn->prompt,
		strlen(conn->prompt));
	if (status == -1)
		return status;

	return 0;
}

static int
control_event_handle(struct softnic_conn *conn,
	int fd_client)
{
	int status;

	status = epoll_ctl(conn->fd_client_group,
		EPOLL_CTL_DEL,
		fd_client,
		NULL);
	if (status == -1)
		return -1;

	status = close(fd_client);
	if (status == -1)
		return -1;

	return 0;
}

int
softnic_conn_poll_for_msg(struct softnic_conn *conn)
{
	struct epoll_event event;
	int fd_client, status, status_data = 0, status_control = 0;

	/* Check input arguments */
	if (conn == NULL)
		return -1;

	/* Client group */
	status = epoll_wait(conn->fd_client_group,
		&event,
		1,
		0);
	if (status == -1)
		return -1;
	if (status == 0)
		return 0;

	fd_client = event.data.fd;

	/* Data available */
	if (event.events & EPOLLIN)
		status_data = data_event_handle(conn, fd_client);

	/* Control events */
	if (event.events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP))
		status_control = control_event_handle(conn, fd_client);

	if (status_data || status_control)
		return -1;

	return 0;
}
