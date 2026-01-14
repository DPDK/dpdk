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
data_event_handle(struct conn *c, int fd_client)
{
	ssize_t len, i, rc = 0;

	/* Read input message */
	len = read(fd_client, c->buf, c->buf_size);
	if (len == -1) {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			return 0;

		return -1;
	}

	if (len == 0)
		return rc;

	/* Handle input messages */
	for (i = 0; i < len; i++) {
		if (c->buf[i] == '\n') {
			size_t n;

			c->msg_in[c->msg_in_len] = 0;
			c->msg_out[0] = 0;

			c->msg_handle(c->msg_in, c->msg_out, c->msg_out_len_max,
					 c->msg_handle_arg);

			n = strlen(c->msg_out);
			if (n) {
				rc = write(fd_client, c->msg_out, n);
				if (rc == -1)
					goto exit;
			}

			c->msg_in_len = 0;
		} else if (c->msg_in_len < c->msg_in_len_max) {
			c->msg_in[c->msg_in_len] = c->buf[i];
			c->msg_in_len++;
		} else {
			rc = write(fd_client, MSG_CMD_TOO_LONG, strlen(MSG_CMD_TOO_LONG));
			if (rc == -1)
				goto exit;

			c->msg_in_len = 0;
		}
	}

	/* Write prompt */
	rc = write(fd_client, c->prompt, strlen(c->prompt));
	rc = (rc == -1) ? -1 : 0;

exit:
	return rc;
}

static int
control_event_handle(struct conn *c, int fd_client)
{
	int rc;

	rc = epoll_ctl(c->fd_client_group, EPOLL_CTL_DEL, fd_client, NULL);
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
	struct conn *c = NULL;
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
	c = calloc(1, sizeof(struct conn));
	if (c == NULL)
		goto exit;

	c->welcome = calloc(1, CONN_WELCOME_LEN_MAX + 1);
	c->prompt = calloc(1, CONN_PROMPT_LEN_MAX + 1);
	c->buf = calloc(1, p->buf_size);
	c->msg_in = calloc(1, p->msg_in_len_max + 1);
	c->msg_out = calloc(1, p->msg_out_len_max + 1);

	if ((c->welcome == NULL) || (c->prompt == NULL) || (c->buf == NULL) ||
	    (c->msg_in == NULL) || (c->msg_out == NULL)) {
		conn_free(c);
		c = NULL;
		goto exit;
	}

	/* Server socket */
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(p->port);

	fd_server = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (fd_server == -1) {
		conn_free(c);
		c = NULL;
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
	rte_strscpy(c->welcome, p->welcome, CONN_WELCOME_LEN_MAX);
	rte_strscpy(c->prompt, p->prompt, CONN_PROMPT_LEN_MAX);
	c->buf_size = p->buf_size;
	c->msg_in_len_max = p->msg_in_len_max;
	c->msg_out_len_max = p->msg_out_len_max;
	c->msg_in_len = 0;
	c->fd_server = fd_server;
	c->fd_client_group = fd_client_group;
	c->msg_handle = p->msg_handle;
	c->msg_handle_arg = p->msg_handle_arg;

exit:
	return c;
free:
	conn_free(c);
	close(fd_server);
	c = NULL;
	return c;
}

void
conn_free(struct conn *c)
{
	if (c == NULL)
		return;

	if (c->fd_client_group)
		close(c->fd_client_group);

	if (c->fd_server)
		close(c->fd_server);

	free(c->msg_out);
	free(c->msg_in);
	free(c->prompt);
	free(c->welcome);
	free(c);
}

int
conn_req_poll(struct conn *c)
{
	struct sockaddr_in client_address;
	socklen_t client_address_length;
	struct epoll_event event;
	int fd_client, rc;

	/* Check input arguments */
	if (c == NULL)
		return -1;

	/* Server socket */
	client_address_length = sizeof(client_address);
	fd_client = accept4(c->fd_server, (struct sockaddr *)&client_address,
			    &client_address_length, SOCK_NONBLOCK);
	if (fd_client == -1) {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			return 0;

		return -1;
	}

	/* Client group */
	event.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP;
	event.data.fd = fd_client;

	rc = epoll_ctl(c->fd_client_group, EPOLL_CTL_ADD, fd_client, &event);
	if (rc == -1) {
		close(fd_client);
		goto exit;
	}

	/* Client */
	rc = write(fd_client, c->welcome, strlen(c->welcome));
	if (rc == -1) {
		close(fd_client);
		goto exit;
	}

	rc = write(fd_client, c->prompt, strlen(c->prompt));
	if (rc == -1) {
		close(fd_client);
		goto exit;
	}

	rc = 0;

exit:
	return rc;
}

int
conn_msg_poll(struct conn *c)
{
	int fd_client, rc, rc_data = 0, rc_control = 0;
	struct epoll_event event;

	/* Check input arguments */
	if (c == NULL)
		return -1;

	/* Client group */
	rc = epoll_wait(c->fd_client_group, &event, 1, 0);
	if ((rc == -1) || rc == 0)
		return rc;

	fd_client = event.data.fd;

	/* Data available */
	if (event.events & EPOLLIN)
		rc_data = data_event_handle(c, fd_client);

	/* Control events */
	if (event.events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP))
		rc_control = control_event_handle(c, fd_client);

	if (rc_data || rc_control)
		return -1;

	return 0;
}
